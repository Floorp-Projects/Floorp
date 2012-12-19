/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <stdio.h>
#include <string.h>

#include "mozilla/DebugOnly.h"

#include "prlink.h"
#include "prenv.h"

#include "nsThreadUtils.h"

#include "gfxPlatform.h"
#include "GLContext.h"
#include "GLContextProvider.h"

#include "gfxCrashReporterUtils.h"
#include "gfxUtils.h"

#include "mozilla/Preferences.h"

#include "GLTextureImage.h"

#include "nsIMemoryReporter.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

#ifdef DEBUG
unsigned GLContext::sCurrentGLContextTLS = -1;
#endif

uint32_t GLContext::sDebugMode = 0;

// define this here since it's global to GLContextProvider, not any
// specific implementation
const ContextFormat ContextFormat::BasicRGBA32Format(ContextFormat::BasicRGBA32);

#define MAX_SYMBOL_LENGTH 128
#define MAX_SYMBOL_NAMES 5

// should match the order of GLExtensions, and be null-terminated.
static const char *sExtensionNames[] = {
    "GL_EXT_framebuffer_object",
    "GL_ARB_framebuffer_object",
    "GL_ARB_texture_rectangle",
    "GL_EXT_bgra",
    "GL_EXT_texture_format_BGRA8888",
    "GL_OES_depth24",
    "GL_OES_depth32",
    "GL_OES_stencil8",
    "GL_OES_texture_npot",
    "GL_OES_depth_texture",
    "GL_OES_packed_depth_stencil",
    "GL_IMG_read_format",
    "GL_EXT_read_format_bgra",
    "GL_APPLE_client_storage",
    "GL_ARB_texture_non_power_of_two",
    "GL_ARB_pixel_buffer_object",
    "GL_ARB_ES2_compatibility",
    "GL_OES_texture_float",
    "GL_ARB_texture_float",
    "GL_EXT_unpack_subimage",
    "GL_OES_standard_derivatives",
    "GL_EXT_texture_filter_anisotropic",
    "GL_EXT_texture_compression_s3tc",
    "GL_EXT_texture_compression_dxt1",
    "GL_ANGLE_texture_compression_dxt3",
    "GL_ANGLE_texture_compression_dxt5",
    "GL_AMD_compressed_ATC_texture",
    "GL_IMG_texture_compression_pvrtc",
    "GL_EXT_framebuffer_blit",
    "GL_ANGLE_framebuffer_blit",
    "GL_EXT_framebuffer_multisample",
    "GL_ANGLE_framebuffer_multisample",
    "GL_OES_rgb8_rgba8",
    "GL_ARB_robustness",
    "GL_EXT_robustness",
    "GL_ARB_sync",
    "GL_OES_EGL_image",
    "GL_OES_EGL_sync",
    "GL_OES_EGL_image_external",
    "GL_EXT_packed_depth_stencil",
    nullptr
};

static int64_t sTextureMemoryUsage = 0;

static int64_t
GetTextureMemoryUsage()
{
    return sTextureMemoryUsage;
}

void
GLContext::UpdateTextureMemoryUsage(MemoryUse action, GLenum format, GLenum type, uint16_t tileSize)
{
    uint32_t bytesPerTexel = mozilla::gl::GetBitsPerTexel(format, type) / 8;
    int64_t bytes = (int64_t)(tileSize * tileSize * bytesPerTexel);
    if (action == MemoryFreed) {
        sTextureMemoryUsage -= bytes;
    } else {
        sTextureMemoryUsage += bytes;
    }
}

NS_MEMORY_REPORTER_IMPLEMENT(TextureMemoryUsage,
    "gfx-textures",
    KIND_OTHER,
    UNITS_BYTES,
    GetTextureMemoryUsage,
    "Memory used for storing GL textures.")

/*
 * XXX - we should really know the ARB/EXT variants of these
 * instead of only handling the symbol if it's exposed directly.
 */

bool
GLContext::InitWithPrefix(const char *prefix, bool trygl)
{
    ScopedGfxFeatureReporter reporter("GL Context");

    if (mInitialized) {
        reporter.SetSuccessful();
        return true;
    }

    mWorkAroundDriverBugs = gfxPlatform::GetPlatform()->WorkAroundDriverBugs();

    SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &mSymbols.fActiveTexture, { "ActiveTexture", "ActiveTextureARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fAttachShader, { "AttachShader", "AttachShaderARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fBindAttribLocation, { "BindAttribLocation", "BindAttribLocationARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fBindBuffer, { "BindBuffer", "BindBufferARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fBindTexture, { "BindTexture", "BindTextureARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fBlendColor, { "BlendColor", NULL } },
        { (PRFuncPtr*) &mSymbols.fBlendEquation, { "BlendEquation", NULL } },
        { (PRFuncPtr*) &mSymbols.fBlendEquationSeparate, { "BlendEquationSeparate", "BlendEquationSeparateEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fBlendFunc, { "BlendFunc", NULL } },
        { (PRFuncPtr*) &mSymbols.fBlendFuncSeparate, { "BlendFuncSeparate", "BlendFuncSeparateEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fBufferData, { "BufferData", NULL } },
        { (PRFuncPtr*) &mSymbols.fBufferSubData, { "BufferSubData", NULL } },
        { (PRFuncPtr*) &mSymbols.fClear, { "Clear", NULL } },
        { (PRFuncPtr*) &mSymbols.fClearColor, { "ClearColor", NULL } },
        { (PRFuncPtr*) &mSymbols.fClearStencil, { "ClearStencil", NULL } },
        { (PRFuncPtr*) &mSymbols.fColorMask, { "ColorMask", NULL } },
        { (PRFuncPtr*) &mSymbols.fCompressedTexImage2D, {"CompressedTexImage2D", NULL} },
        { (PRFuncPtr*) &mSymbols.fCompressedTexSubImage2D, {"CompressedTexSubImage2D", NULL} },
        { (PRFuncPtr*) &mSymbols.fCullFace, { "CullFace", NULL } },
        { (PRFuncPtr*) &mSymbols.fDetachShader, { "DetachShader", "DetachShaderARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fDepthFunc, { "DepthFunc", NULL } },
        { (PRFuncPtr*) &mSymbols.fDepthMask, { "DepthMask", NULL } },
        { (PRFuncPtr*) &mSymbols.fDisable, { "Disable", NULL } },
        { (PRFuncPtr*) &mSymbols.fDisableVertexAttribArray, { "DisableVertexAttribArray", "DisableVertexAttribArrayARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fDrawArrays, { "DrawArrays", NULL } },
        { (PRFuncPtr*) &mSymbols.fDrawElements, { "DrawElements", NULL } },
        { (PRFuncPtr*) &mSymbols.fEnable, { "Enable", NULL } },
        { (PRFuncPtr*) &mSymbols.fEnableVertexAttribArray, { "EnableVertexAttribArray", "EnableVertexAttribArrayARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fFinish, { "Finish", NULL } },
        { (PRFuncPtr*) &mSymbols.fFlush, { "Flush", NULL } },
        { (PRFuncPtr*) &mSymbols.fFrontFace, { "FrontFace", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetActiveAttrib, { "GetActiveAttrib", "GetActiveAttribARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetActiveUniform, { "GetActiveUniform", "GetActiveUniformARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetAttachedShaders, { "GetAttachedShaders", "GetAttachedShadersARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetAttribLocation, { "GetAttribLocation", "GetAttribLocationARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetIntegerv, { "GetIntegerv", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetFloatv, { "GetFloatv", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetBooleanv, { "GetBooleanv", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetBufferParameteriv, { "GetBufferParameteriv", "GetBufferParameterivARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetError, { "GetError", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetProgramiv, { "GetProgramiv", "GetProgramivARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetProgramInfoLog, { "GetProgramInfoLog", "GetProgramInfoLogARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fTexParameteri, { "TexParameteri", NULL } },
        { (PRFuncPtr*) &mSymbols.fTexParameterf, { "TexParameterf", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetString, { "GetString", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetTexParameterfv, { "GetTexParameterfv", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetTexParameteriv, { "GetTexParameteriv", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetUniformfv, { "GetUniformfv", "GetUniformfvARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetUniformiv, { "GetUniformiv", "GetUniformivARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetUniformLocation, { "GetUniformLocation", "GetUniformLocationARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetVertexAttribfv, { "GetVertexAttribfv", "GetVertexAttribfvARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetVertexAttribiv, { "GetVertexAttribiv", "GetVertexAttribivARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetVertexAttribPointerv, { "GetVertexAttribPointerv", NULL } },
        { (PRFuncPtr*) &mSymbols.fHint, { "Hint", NULL } },
        { (PRFuncPtr*) &mSymbols.fIsBuffer, { "IsBuffer", "IsBufferARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fIsEnabled, { "IsEnabled", NULL } },
        { (PRFuncPtr*) &mSymbols.fIsProgram, { "IsProgram", "IsProgramARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fIsShader, { "IsShader", "IsShaderARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fIsTexture, { "IsTexture", "IsTextureARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fLineWidth, { "LineWidth", NULL } },
        { (PRFuncPtr*) &mSymbols.fLinkProgram, { "LinkProgram", "LinkProgramARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fPixelStorei, { "PixelStorei", NULL } },
        { (PRFuncPtr*) &mSymbols.fPolygonOffset, { "PolygonOffset", NULL } },
        { (PRFuncPtr*) &mSymbols.fReadPixels, { "ReadPixels", NULL } },
        { (PRFuncPtr*) &mSymbols.fSampleCoverage, { "SampleCoverage", NULL } },
        { (PRFuncPtr*) &mSymbols.fScissor, { "Scissor", NULL } },
        { (PRFuncPtr*) &mSymbols.fStencilFunc, { "StencilFunc", NULL } },
        { (PRFuncPtr*) &mSymbols.fStencilFuncSeparate, { "StencilFuncSeparate", "StencilFuncSeparateEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fStencilMask, { "StencilMask", NULL } },
        { (PRFuncPtr*) &mSymbols.fStencilMaskSeparate, { "StencilMaskSeparate", "StencilMaskSeparateEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fStencilOp, { "StencilOp", NULL } },
        { (PRFuncPtr*) &mSymbols.fStencilOpSeparate, { "StencilOpSeparate", "StencilOpSeparateEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fTexImage2D, { "TexImage2D", NULL } },
        { (PRFuncPtr*) &mSymbols.fTexSubImage2D, { "TexSubImage2D", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform1f, { "Uniform1f", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform1fv, { "Uniform1fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform1i, { "Uniform1i", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform1iv, { "Uniform1iv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform2f, { "Uniform2f", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform2fv, { "Uniform2fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform2i, { "Uniform2i", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform2iv, { "Uniform2iv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform3f, { "Uniform3f", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform3fv, { "Uniform3fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform3i, { "Uniform3i", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform3iv, { "Uniform3iv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform4f, { "Uniform4f", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform4fv, { "Uniform4fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform4i, { "Uniform4i", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniform4iv, { "Uniform4iv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniformMatrix2fv, { "UniformMatrix2fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniformMatrix3fv, { "UniformMatrix3fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUniformMatrix4fv, { "UniformMatrix4fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fUseProgram, { "UseProgram", NULL } },
        { (PRFuncPtr*) &mSymbols.fValidateProgram, { "ValidateProgram", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttribPointer, { "VertexAttribPointer", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib1f, { "VertexAttrib1f", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib2f, { "VertexAttrib2f", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib3f, { "VertexAttrib3f", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib4f, { "VertexAttrib4f", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib1fv, { "VertexAttrib1fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib2fv, { "VertexAttrib2fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib3fv, { "VertexAttrib3fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib4fv, { "VertexAttrib4fv", NULL } },
        { (PRFuncPtr*) &mSymbols.fViewport, { "Viewport", NULL } },
        { (PRFuncPtr*) &mSymbols.fCompileShader, { "CompileShader", NULL } },
        { (PRFuncPtr*) &mSymbols.fCopyTexImage2D, { "CopyTexImage2D", NULL } },
        { (PRFuncPtr*) &mSymbols.fCopyTexSubImage2D, { "CopyTexSubImage2D", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetShaderiv, { "GetShaderiv", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetShaderInfoLog, { "GetShaderInfoLog", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetShaderSource, { "GetShaderSource", NULL } },
        { (PRFuncPtr*) &mSymbols.fShaderSource, { "ShaderSource", NULL } },
        { (PRFuncPtr*) &mSymbols.fVertexAttribPointer, { "VertexAttribPointer", NULL } },
        { (PRFuncPtr*) &mSymbols.fBindFramebuffer, { "BindFramebuffer", "BindFramebufferEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fBindRenderbuffer, { "BindRenderbuffer", "BindRenderbufferEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fCheckFramebufferStatus, { "CheckFramebufferStatus", "CheckFramebufferStatusEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fFramebufferRenderbuffer, { "FramebufferRenderbuffer", "FramebufferRenderbufferEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fFramebufferTexture2D, { "FramebufferTexture2D", "FramebufferTexture2DEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fGenerateMipmap, { "GenerateMipmap", "GenerateMipmapEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetFramebufferAttachmentParameteriv, { "GetFramebufferAttachmentParameteriv", "GetFramebufferAttachmentParameterivEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fGetRenderbufferParameteriv, { "GetRenderbufferParameteriv", "GetRenderbufferParameterivEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fIsFramebuffer, { "IsFramebuffer", "IsFramebufferEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fIsRenderbuffer, { "IsRenderbuffer", "IsRenderbufferEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fRenderbufferStorage, { "RenderbufferStorage", "RenderbufferStorageEXT", NULL } },

        { (PRFuncPtr*) &mSymbols.fGenBuffers, { "GenBuffers", "GenBuffersARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGenTextures, { "GenTextures", NULL } },
        { (PRFuncPtr*) &mSymbols.fCreateProgram, { "CreateProgram", "CreateProgramARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fCreateShader, { "CreateShader", "CreateShaderARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fGenFramebuffers, { "GenFramebuffers", "GenFramebuffersEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fGenRenderbuffers, { "GenRenderbuffers", "GenRenderbuffersEXT", NULL } },

        { (PRFuncPtr*) &mSymbols.fDeleteBuffers, { "DeleteBuffers", "DeleteBuffersARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fDeleteTextures, { "DeleteTextures", "DeleteTexturesARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fDeleteProgram, { "DeleteProgram", "DeleteProgramARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fDeleteShader, { "DeleteShader", "DeleteShaderARB", NULL } },
        { (PRFuncPtr*) &mSymbols.fDeleteFramebuffers, { "DeleteFramebuffers", "DeleteFramebuffersEXT", NULL } },
        { (PRFuncPtr*) &mSymbols.fDeleteRenderbuffers, { "DeleteRenderbuffers", "DeleteRenderbuffersEXT", NULL } },

        { NULL, { NULL } },

    };

    mInitialized = LoadSymbols(&symbols[0], trygl, prefix);

    // Load OpenGL ES 2.0 symbols, or desktop if we aren't using ES 2.
    if (mInitialized) {
        if (mIsGLES2) {
            SymLoadStruct symbols_ES2[] = {
                { (PRFuncPtr*) &mSymbols.fGetShaderPrecisionFormat, { "GetShaderPrecisionFormat", NULL } },
                { (PRFuncPtr*) &mSymbols.fClearDepthf, { "ClearDepthf", NULL } },
                { (PRFuncPtr*) &mSymbols.fDepthRangef, { "DepthRangef", NULL } },
                { NULL, { NULL } },
            };

            if (!LoadSymbols(&symbols_ES2[0], trygl, prefix)) {
                NS_ERROR("OpenGL ES 2.0 supported, but symbols could not be loaded.");
                mInitialized = false;
            }
        } else {
            SymLoadStruct symbols_desktop[] = {
                { (PRFuncPtr*) &mSymbols.fClearDepth, { "ClearDepth", NULL } },
                { (PRFuncPtr*) &mSymbols.fDepthRange, { "DepthRange", NULL } },
                { (PRFuncPtr*) &mSymbols.fReadBuffer, { "ReadBuffer", NULL } },
                { (PRFuncPtr*) &mSymbols.fMapBuffer, { "MapBuffer", NULL } },
                { (PRFuncPtr*) &mSymbols.fUnmapBuffer, { "UnmapBuffer", NULL } },
                { (PRFuncPtr*) &mSymbols.fPointParameterf, { "PointParameterf", NULL } },
                { NULL, { NULL } },
            };

            if (!LoadSymbols(&symbols_desktop[0], trygl, prefix)) {
                NS_ERROR("Desktop symbols failed to load.");
                mInitialized = false;
            }
        }
    }

    const char *glVendorString;
    const char *glRendererString;

    if (mInitialized) {
        // The order of these strings must match up with the order of the enum
        // defined in GLContext.h for vendor IDs
        glVendorString = (const char *)fGetString(LOCAL_GL_VENDOR);
        const char *vendorMatchStrings[VendorOther] = {
                "Intel",
                "NVIDIA",
                "ATI",
                "Qualcomm",
                "Imagination",
                "nouveau"
        };
        mVendor = VendorOther;
        for (int i = 0; i < VendorOther; ++i) {
            if (DoesStringMatch(glVendorString, vendorMatchStrings[i])) {
                mVendor = i;
                break;
            }
        }

        // The order of these strings must match up with the order of the enum
        // defined in GLContext.h for renderer IDs
        glRendererString = (const char *)fGetString(LOCAL_GL_RENDERER);
        const char *rendererMatchStrings[RendererOther] = {
                "Adreno 200",
                "Adreno 205",
                "PowerVR SGX 530",
                "PowerVR SGX 540",

        };
        mRenderer = RendererOther;
        for (int i = 0; i < RendererOther; ++i) {
            if (DoesStringMatch(glRendererString, rendererMatchStrings[i])) {
                mRenderer = i;
                break;
            }
        }
    }

#ifdef DEBUG
    if (PR_GetEnv("MOZ_GL_DEBUG"))
        sDebugMode |= DebugEnabled;

    // enables extra verbose output, informing of the start and finish of every GL call.
    // useful e.g. to record information to investigate graphics system crashes/lockups
    if (PR_GetEnv("MOZ_GL_DEBUG_VERBOSE"))
        sDebugMode |= DebugTrace;

    // aborts on GL error. Can be useful to debug quicker code that is known not to generate any GL error in principle.
    if (PR_GetEnv("MOZ_GL_DEBUG_ABORT_ON_ERROR"))
        sDebugMode |= DebugAbortOnError;
#endif

    if (mInitialized) {
#ifdef DEBUG
        static bool firstRun = true;
        if (firstRun && DebugMode()) {
            const char *vendors[VendorOther] = {
                "Intel",
                "NVIDIA",
                "ATI",
                "Qualcomm"
            };

            if (mVendor < VendorOther) {
                printf_stderr("OpenGL vendor ('%s') recognized as: %s\n",
                              glVendorString, vendors[mVendor]);
            } else {
                printf_stderr("OpenGL vendor ('%s') unrecognized\n", glVendorString);
            }
        }
        firstRun = false;
#endif

        InitExtensions();

        NS_ASSERTION(!IsExtensionSupported(GLContext::ARB_pixel_buffer_object) ||
                     (mSymbols.fMapBuffer && mSymbols.fUnmapBuffer),
                     "ARB_pixel_buffer_object supported without glMapBuffer/UnmapBuffer being available!");

        if (SupportsRobustness()) {
            if (IsExtensionSupported(ARB_robustness)) {
                SymLoadStruct robustnessSymbols[] = {
                    { (PRFuncPtr*) &mSymbols.fGetGraphicsResetStatus, { "GetGraphicsResetStatusARB", nullptr } },
                    { nullptr, { nullptr } },
                };

                if (!LoadSymbols(&robustnessSymbols[0], trygl, prefix)) {
                    NS_ERROR("GL supports ARB_robustness without supplying GetGraphicsResetStatusARB.");

                    MarkExtensionUnsupported(ARB_robustness);
                    mSymbols.fGetGraphicsResetStatus = nullptr;
                } else {
                    mHasRobustness = true;
                }
            }
            if (!IsExtensionSupported(ARB_robustness) &&
                IsExtensionSupported(EXT_robustness)) {
                SymLoadStruct robustnessSymbols[] = {
                    { (PRFuncPtr*) &mSymbols.fGetGraphicsResetStatus, { "GetGraphicsResetStatusEXT", nullptr } },
                    { nullptr, { nullptr } },
                };

                if (!LoadSymbols(&robustnessSymbols[0], trygl, prefix)) {
                    NS_ERROR("GL supports EXT_robustness without supplying GetGraphicsResetStatusEXT.");

                    MarkExtensionUnsupported(EXT_robustness);
                    mSymbols.fGetGraphicsResetStatus = nullptr;
                } else {
                    mHasRobustness = true;
                }
            }
        }

        // Check for aux symbols based on extensions
        if (IsExtensionSupported(GLContext::ANGLE_framebuffer_blit) ||
            IsExtensionSupported(GLContext::EXT_framebuffer_blit))
        {
            SymLoadStruct auxSymbols[] = {
                {
                    (PRFuncPtr*) &mSymbols.fBlitFramebuffer,
                    {
                        "BlitFramebuffer",
                        "BlitFramebufferEXT",
                        "BlitFramebufferANGLE",
                        nullptr
                    }
                },
                { nullptr, { nullptr } },
            };
            if (!LoadSymbols(&auxSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports framebuffer_blit without supplying glBlitFramebuffer");

                MarkExtensionUnsupported(ANGLE_framebuffer_blit);
                MarkExtensionUnsupported(EXT_framebuffer_blit);
                mSymbols.fBlitFramebuffer = nullptr;
            }
        }

        if (SupportsOffscreenSplit() &&
            ( IsExtensionSupported(GLContext::ANGLE_framebuffer_multisample) ||
              IsExtensionSupported(GLContext::EXT_framebuffer_multisample) ))
        {
            SymLoadStruct auxSymbols[] = {
                {
                    (PRFuncPtr*) &mSymbols.fRenderbufferStorageMultisample,
                    {
                        "RenderbufferStorageMultisample",
                        "RenderbufferStorageMultisampleEXT",
                        "RenderbufferStorageMultisampleANGLE",
                        nullptr
                    }
                },
                { nullptr, { nullptr } },
            };
            if (!LoadSymbols(&auxSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports framebuffer_multisample without supplying glRenderbufferStorageMultisample");

                MarkExtensionUnsupported(ANGLE_framebuffer_multisample);
                MarkExtensionUnsupported(EXT_framebuffer_multisample);
                mSymbols.fRenderbufferStorageMultisample = nullptr;
            }
        }

        if (IsExtensionSupported(ARB_sync)) {
            SymLoadStruct syncSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fFenceSync,      { "FenceSync",      nullptr } },
                { (PRFuncPtr*) &mSymbols.fIsSync,         { "IsSync",         nullptr } },
                { (PRFuncPtr*) &mSymbols.fDeleteSync,     { "DeleteSync",     nullptr } },
                { (PRFuncPtr*) &mSymbols.fClientWaitSync, { "ClientWaitSync", nullptr } },
                { (PRFuncPtr*) &mSymbols.fWaitSync,       { "WaitSync",       nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetInteger64v,  { "GetInteger64v",  nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetSynciv,      { "GetSynciv",      nullptr } },
                { nullptr, { nullptr } },
            };

            if (!LoadSymbols(&syncSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports ARB_sync without supplying its functions.");

                MarkExtensionUnsupported(ARB_sync);
                mSymbols.fFenceSync = nullptr;
                mSymbols.fIsSync = nullptr;
                mSymbols.fDeleteSync = nullptr;
                mSymbols.fClientWaitSync = nullptr;
                mSymbols.fWaitSync = nullptr;
                mSymbols.fGetInteger64v = nullptr;
                mSymbols.fGetSynciv = nullptr;
            }
        }

        if (IsExtensionSupported(OES_EGL_image)) {
            SymLoadStruct imageSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fEGLImageTargetTexture2D, { "EGLImageTargetTexture2DOES", nullptr } },
                { nullptr, { nullptr } },
            };

            if (!LoadSymbols(&imageSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports OES_EGL_image without supplying its functions.");

                MarkExtensionUnsupported(OES_EGL_image);
                mSymbols.fEGLImageTargetTexture2D = nullptr;
            }
        }
       
        // Load developer symbols, don't fail if we can't find them.
        SymLoadStruct auxSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetTexImage, { "GetTexImage", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetTexLevelParameteriv, { "GetTexLevelParameteriv", nullptr } },
                { nullptr, { nullptr } },
        };
        bool warnOnFailures = DebugMode();
        LoadSymbols(&auxSymbols[0], trygl, prefix, warnOnFailures);
    }

    if (mInitialized) {
        GLint v[4];

        fGetIntegerv(LOCAL_GL_SCISSOR_BOX, v);
        mScissorStack.AppendElement(nsIntRect(v[0], v[1], v[2], v[3]));

        fGetIntegerv(LOCAL_GL_VIEWPORT, v);
        mViewportStack.AppendElement(nsIntRect(v[0], v[1], v[2], v[3]));

        raw_fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
        raw_fGetIntegerv(LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE, &mMaxCubeMapTextureSize);
        raw_fGetIntegerv(LOCAL_GL_MAX_RENDERBUFFER_SIZE, &mMaxRenderbufferSize);

#ifdef XP_MACOSX
        if (mWorkAroundDriverBugs &&
            mVendor == VendorIntel) {
            // see bug 737182 for 2D textures, bug 684882 for cube map textures.
            mMaxTextureSize        = NS_MIN(mMaxTextureSize,        4096);
            mMaxCubeMapTextureSize = NS_MIN(mMaxCubeMapTextureSize, 512);
            // for good measure, we align renderbuffers on what we do for 2D textures
            mMaxRenderbufferSize   = NS_MIN(mMaxRenderbufferSize,   4096);
            mNeedsTextureSizeChecks = true;
        }
#endif
#ifdef MOZ_X11
        if (mWorkAroundDriverBugs &&
            mVendor == VendorNouveau) {
            // see bug 814716. Clamp MaxCubeMapTextureSize at 2K for Nouveau.
            mMaxCubeMapTextureSize = NS_MIN(mMaxCubeMapTextureSize, 2048);
            mNeedsTextureSizeChecks = true;
        }
#endif

        mMaxTextureImageSize = mMaxTextureSize;

        UpdateActualFormat();
    }

    if (mInitialized)
        reporter.SetSuccessful();
    else {
        // if initialization fails, ensure all symbols are zero, to avoid hard-to-understand bugs
        mSymbols.Zero();
        NS_WARNING("InitWithPrefix failed!");
    }

    return mInitialized;
}

void
GLContext::InitExtensions()
{
    MakeCurrent();
    const char* extensions = (const char*)fGetString(LOCAL_GL_EXTENSIONS);
    if (!extensions)
        return;

#ifdef DEBUG
    static bool firstRun = true;
#else
    // Non-DEBUG, so never spew.
    const bool firstRun = false;
#endif

    mAvailableExtensions.Load(extensions, sExtensionNames, firstRun && DebugMode());

#ifdef XP_MACOSX
    // The Mac Nvidia driver, for versions up to and including 10.8, don't seem
    // to properly support this.  See 814839
    if (WorkAroundDriverBugs() &&
        Vendor() == gl::GLContext::VendorNVIDIA)
    {
        MarkExtensionUnsupported(gl::GLContext::EXT_packed_depth_stencil);
    }
#endif

#ifdef DEBUG
    firstRun = false;
#endif
}


// Take texture data in a given buffer and copy it into a larger buffer,
// padding out the edge pixels for filtering if necessary
static void
CopyAndPadTextureData(const GLvoid* srcBuffer,
                      GLvoid* dstBuffer,
                      GLsizei srcWidth, GLsizei srcHeight,
                      GLsizei dstWidth, GLsizei dstHeight,
                      GLsizei stride, GLint pixelsize)
{
    unsigned char *rowDest = static_cast<unsigned char*>(dstBuffer);
    const unsigned char *source = static_cast<const unsigned char*>(srcBuffer);

    for (GLsizei h = 0; h < srcHeight; ++h) {
        memcpy(rowDest, source, srcWidth * pixelsize);
        rowDest += dstWidth * pixelsize;
        source += stride;
    }

    GLsizei padHeight = srcHeight;

    // Pad out an extra row of pixels so that edge filtering doesn't use garbage data
    if (dstHeight > srcHeight) {
        memcpy(rowDest, source - stride, srcWidth * pixelsize);
        padHeight++;
    }

    // Pad out an extra column of pixels
    if (dstWidth > srcWidth) {
        rowDest = static_cast<unsigned char*>(dstBuffer) + srcWidth * pixelsize;
        for (GLsizei h = 0; h < padHeight; ++h) {
            memcpy(rowDest, rowDest - pixelsize, pixelsize);
            rowDest += dstWidth * pixelsize;
        }
    }
}

// In both of these cases (for the Adreno at least) it is impossible
// to determine good or bad driver versions for POT texture uploads,
// so blacklist them all. Newer drivers use a different rendering
// string in the form "Adreno (TM) 200" and the drivers we've seen so
// far work fine with NPOT textures, so don't blacklist those until we
// have evidence of any problems with them.
bool
GLContext::CanUploadSubTextures()
{
    if (!mWorkAroundDriverBugs)
        return true;

    // Lock surface feature allows to mmap texture memory and modify it directly
    // this feature allow us modify texture partially without full upload
    if (HasLockSurface())
        return true;

    // There are certain GPUs that we don't want to use glTexSubImage2D on
    // because that function can be very slow and/or buggy
    if (Renderer() == RendererAdreno200 || Renderer() == RendererAdreno205)
        return false;

    // On PowerVR glTexSubImage does a readback, so it will be slower
    // than just doing a glTexImage2D() directly. i.e. 26ms vs 10ms
    if (Renderer() == RendererSGX540 || Renderer() == RendererSGX530)
        return false;

    return true;
}

bool GLContext::sPowerOfTwoForced = false;
bool GLContext::sPowerOfTwoPrefCached = false;

void
GLContext::PlatformStartup()
{
  CacheCanUploadNPOT();
  NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(TextureMemoryUsage));
}

void
GLContext::CacheCanUploadNPOT()
{
    MOZ_ASSERT(NS_IsMainThread(), "Can't cache prefs off the main thread.");
    MOZ_ASSERT(!sPowerOfTwoPrefCached, "Must only call this function once!");

    sPowerOfTwoPrefCached = true;
    mozilla::Preferences::AddBoolVarCache(&sPowerOfTwoForced,
                                          "gfx.textures.poweroftwo.force-enabled");
}

bool
GLContext::CanUploadNonPowerOfTwo()
{
    MOZ_ASSERT(sPowerOfTwoPrefCached);

    if (!mWorkAroundDriverBugs)
        return true;

    // Some GPUs driver crash when uploading non power of two 565 textures.
    return sPowerOfTwoForced ? false : (Renderer() != RendererAdreno200 &&
                                        Renderer() != RendererAdreno205);
}

bool
GLContext::WantsSmallTiles()
{
    // We must use small tiles for good performance if we can't use
    // glTexSubImage2D() for some reason.
    if (!CanUploadSubTextures())
        return true;

    // We can't use small tiles on the SGX 540, because of races in texture upload.
    if (mWorkAroundDriverBugs &&
        Renderer() == RendererSGX540)
        return false;

    // Don't use small tiles otherwise. (If we implement incremental texture upload,
    // then we will want to revisit this.)
    return false;
}

// Common code for checking for both GL extensions and GLX extensions.
bool
GLContext::ListHasExtension(const GLubyte *extensions, const char *extension)
{
    // fix bug 612572 - we were crashing as we were calling this function with extensions==null
    if (extensions == nullptr || extension == nullptr)
        return false;

    const GLubyte *start;
    GLubyte *where, *terminator;

    /* Extension names should not have spaces. */
    where = (GLubyte *) strchr(extension, ' ');
    if (where || *extension == '\0')
        return false;

    /* 
     * It takes a bit of care to be fool-proof about parsing the
     * OpenGL extensions string. Don't be fooled by sub-strings,
     * etc. 
     */
    start = extensions;
    for (;;) {
        where = (GLubyte *) strstr((const char *) start, extension);
        if (!where) {
            break;
        }
        terminator = where + strlen(extension);
        if (where == start || *(where - 1) == ' ') {
            if (*terminator == ' ' || *terminator == '\0') {
                return true;
            }
        }
        start = terminator;
    }
    return false;
}

already_AddRefed<TextureImage>
GLContext::CreateTextureImage(const nsIntSize& aSize,
                              TextureImage::ContentType aContentType,
                              GLenum aWrapMode,
                              TextureImage::Flags aFlags)
{
    bool useNearestFilter = aFlags & TextureImage::UseNearestFilter;
    MakeCurrent();

    GLuint texture;
    fGenTextures(1, &texture);

    fActiveTexture(LOCAL_GL_TEXTURE0);
    fBindTexture(LOCAL_GL_TEXTURE_2D, texture);

    GLint texfilter = useNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, aWrapMode);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, aWrapMode);

    return CreateBasicTextureImage(texture, aSize, aWrapMode, aContentType, this, aFlags);
}

already_AddRefed<TextureImage>
GLContext::CreateBasicTextureImage(GLuint aTexture,
                        const nsIntSize& aSize,
                        GLenum aWrapMode,
                        TextureImage::ContentType aContentType,
                        GLContext* aContext,
                        TextureImage::Flags aFlags)
{
    nsRefPtr<BasicTextureImage> teximage(
        new BasicTextureImage(aTexture, aSize, aWrapMode, aContentType, aContext, aFlags));
    return teximage.forget();
}

void GLContext::ApplyFilterToBoundTexture(gfxPattern::GraphicsFilter aFilter)
{
    ApplyFilterToBoundTexture(LOCAL_GL_TEXTURE_2D, aFilter);
}

void GLContext::ApplyFilterToBoundTexture(GLuint aTarget,
                                          gfxPattern::GraphicsFilter aFilter)
{
    if (aFilter == gfxPattern::FILTER_NEAREST) {
        fTexParameteri(aTarget, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST);
        fTexParameteri(aTarget, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_NEAREST);
    } else {
        fTexParameteri(aTarget, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
        fTexParameteri(aTarget, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    }
}


GLContext::GLFormats
GLContext::ChooseGLFormats(ContextFormat& aCF, ColorByteOrder aByteOrder)
{
    GLFormats formats;

    // If we're on ES2 hardware and we have an explicit request for 16 bits of color or less
    // OR we don't support full 8-bit color, return a 4444 or 565 format.
    if (mIsGLES2 && (aCF.colorBits() <= 16 || !IsExtensionSupported(OES_rgb8_rgba8))) {
        if (aCF.alpha) {
            formats.texColor = LOCAL_GL_RGBA;
            formats.texColorType = LOCAL_GL_UNSIGNED_SHORT_4_4_4_4;
            formats.rbColor = LOCAL_GL_RGBA4;

            aCF.red = aCF.green = aCF.blue = aCF.alpha = 4;
        } else {
            formats.texColor = LOCAL_GL_RGB;
            formats.texColorType = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
            formats.rbColor = LOCAL_GL_RGB565;

            aCF.red = 5;
            aCF.green = 6;
            aCF.blue = 5;
            aCF.alpha = 0;
        }   
    } else {
        formats.texColorType = LOCAL_GL_UNSIGNED_BYTE;

        if (aCF.alpha) {
            // Prefer BGRA8888 on ES2 hardware; if the extension is supported, it
            // should be faster.  There are some cases where we don't want this --
            // specifically, CopyTex*Image doesn't seem to understand how to deal
            // with a BGRA source going to a RGB/RGBA destination on some drivers.
            if (mIsGLES2 &&
                IsExtensionSupported(EXT_texture_format_BGRA8888) &&
                aByteOrder != ForceRGBA)
            {
                formats.texColor = LOCAL_GL_BGRA;
            } else {
                formats.texColor = LOCAL_GL_RGBA;
            }

            formats.rbColor = LOCAL_GL_RGBA8;

            aCF.red = aCF.green = aCF.blue = aCF.alpha = 8;
        } else {
            formats.texColor = LOCAL_GL_RGB;
            formats.rbColor = LOCAL_GL_RGB8;

            aCF.red = aCF.green = aCF.blue = 8;
            aCF.alpha = 0;
        }
    }

    GLsizei samples = aCF.samples;

    GLsizei maxSamples = 0;
    if (SupportsFramebufferMultisample())
        fGetIntegerv(LOCAL_GL_MAX_SAMPLES, (GLint*)&maxSamples);
    samples = NS_MIN(samples, maxSamples);

    // bug 778765
    if (WorkAroundDriverBugs() && samples == 1) {
        samples = 0;
    }

    formats.samples = samples;
    aCF.samples = samples;


    const int depth = aCF.depth;
    const int stencil = aCF.stencil;
    const bool useDepthStencil =
        !mIsGLES2 || IsExtensionSupported(OES_packed_depth_stencil);

    formats.depthStencil = 0;
    formats.depth = 0;
    formats.stencil = 0;
    if (depth && stencil && useDepthStencil) {
        formats.depthStencil = LOCAL_GL_DEPTH24_STENCIL8;
        aCF.depth = 24;
        aCF.stencil = 8;
    } else {
        if (depth) {
            if (mIsGLES2) {
                if (IsExtensionSupported(OES_depth24)) {
                    formats.depth = LOCAL_GL_DEPTH_COMPONENT24;
                    aCF.depth = 24;
                } else {
                    formats.depth = LOCAL_GL_DEPTH_COMPONENT16;
                    aCF.depth = 16;
                }
            } else {
                formats.depth = LOCAL_GL_DEPTH_COMPONENT24;
                aCF.depth = 24;
            }
        }

        if (stencil) {
            formats.stencil = LOCAL_GL_STENCIL_INDEX8;
            aCF.stencil = 8;
        }
    }

    return formats;
}

void
GLContext::CreateTextureForOffscreen(const GLFormats& aFormats, const gfxIntSize& aSize, GLuint& texture)
{
    GLuint boundTexture = 0;
    fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, (GLint*)&boundTexture);

    if (texture == 0) {
        fGenTextures(1, &texture);
    }

    fBindTexture(LOCAL_GL_TEXTURE_2D, texture);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

    fTexImage2D(LOCAL_GL_TEXTURE_2D,
                0,
                aFormats.texColor,
                aSize.width, aSize.height,
                0,
                aFormats.texColor,
                aFormats.texColorType,
                nullptr);

    fBindTexture(LOCAL_GL_TEXTURE_2D, boundTexture);
}

static inline void
RenderbufferStorageBySamples(GLContext* gl, GLsizei samples, GLenum internalFormat, const gfxIntSize& size)
{
    if (samples) {
        gl->fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER,
                                            samples,
                                            internalFormat,
                                            size.width, size.height);
    } else {
        gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER,
                                 internalFormat,
                                 size.width, size.height);
    }
}

void
GLContext::CreateRenderbuffersForOffscreen(const GLContext::GLFormats& aFormats, const gfxIntSize& aSize,
                                           GLuint& colorMSRB, GLuint& depthRB, GLuint& stencilRB)
{
    GLuint boundRB = 0;
    fGetIntegerv(LOCAL_GL_RENDERBUFFER_BINDING, (GLint*)&boundRB);


    colorMSRB = 0;
    depthRB = 0;
    stencilRB = 0;

    if (aFormats.samples > 0) {
        fGenRenderbuffers(1, &colorMSRB);
        fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, colorMSRB);
        RenderbufferStorageBySamples(this, aFormats.samples, aFormats.rbColor, aSize);
    }

    // If depthStencil, disallow depth, stencil
    MOZ_ASSERT(!aFormats.depthStencil || (!aFormats.depth && !aFormats.stencil));

    if (aFormats.depthStencil) {
        fGenRenderbuffers(1, &depthRB);
        stencilRB = depthRB;
        fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, depthRB);
        RenderbufferStorageBySamples(this, aFormats.samples, aFormats.depthStencil, aSize);
    }

    if (aFormats.depth) {
        fGenRenderbuffers(1, &depthRB);
        fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, depthRB);
        RenderbufferStorageBySamples(this, aFormats.samples, aFormats.depth, aSize);
    }

    if (aFormats.stencil) {
        fGenRenderbuffers(1, &stencilRB);
        fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, stencilRB);
        RenderbufferStorageBySamples(this, aFormats.samples, aFormats.stencil, aSize);
    }


    fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, boundRB);
}

bool
GLContext::AssembleOffscreenFBOs(const GLuint colorMSRB,
                                 const GLuint depthRB,
                                 const GLuint stencilRB,
                                 const GLuint texture,
                                 GLuint& drawFBO,
                                 GLuint& readFBO)
{
    drawFBO = 0;
    readFBO = 0;

    if (!colorMSRB && !texture) {
        MOZ_ASSERT(!depthRB && !stencilRB);
        return true;
    }

    GLuint boundDrawFBO = GetUserBoundDrawFBO();
    GLuint boundReadFBO = GetUserBoundReadFBO();

    if (texture) {
        fGenFramebuffers(1, &readFBO);
        BindInternalFBO(readFBO);
        fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                              LOCAL_GL_COLOR_ATTACHMENT0,
                              LOCAL_GL_TEXTURE_2D,
                              texture,
                              0);
    }

    if (colorMSRB) {
        fGenFramebuffers(1, &drawFBO);
        BindInternalFBO(drawFBO);
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_COLOR_ATTACHMENT0,
                                 LOCAL_GL_RENDERBUFFER,
                                 colorMSRB);
    } else {
        drawFBO = readFBO;
        // drawFBO==readFBO is already bound from the 'if (texture)' block.
    }

    if (depthRB) {
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER,
                                 depthRB);
    }

    if (stencilRB) {
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_STENCIL_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER,
                                 stencilRB);
    }

    // We should be all resized.  Check for framebuffer completeness.
    GLenum status;
    bool isComplete = true;

    BindInternalFBO(drawFBO);
    status = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        NS_WARNING("DrawFBO: Incomplete");
  #ifdef DEBUG
        if (DebugMode()) {
            printf_stderr("Framebuffer status: %X\n", status);
        }
  #endif
        isComplete = false;
    }

    BindInternalFBO(readFBO);
    status = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        NS_WARNING("ReadFBO: Incomplete");
  #ifdef DEBUG
        if (DebugMode()) {
            printf_stderr("Framebuffer status: %X\n", status);
        }
  #endif
        isComplete = false;
    }

    BindUserDrawFBO(boundDrawFBO);
    BindUserReadFBO(boundReadFBO);

    return isComplete;
}

bool
GLContext::ResizeOffscreenFBOs(const ContextFormat& aCF, const gfxIntSize& aSize, const bool aNeedsReadBuffer)
{
    // Early out for when we're rendering directly to the context's 'screen'.
    if (!aNeedsReadBuffer && !aCF.samples)
        return true;

    MakeCurrent();
    ContextFormat cf(aCF);
    GLFormats formats = ChooseGLFormats(cf);

    GLuint texture = 0;
    if (aNeedsReadBuffer)
        CreateTextureForOffscreen(formats, aSize, texture);

    GLuint colorMSRB = 0;
    GLuint depthRB = 0;
    GLuint stencilRB = 0;
    CreateRenderbuffersForOffscreen(formats, aSize, colorMSRB, depthRB, stencilRB);

    GLuint drawFBO = 0;
    GLuint readFBO = 0;
    if (!AssembleOffscreenFBOs(colorMSRB, depthRB, stencilRB, texture,
                               drawFBO, readFBO))
    {
        fDeleteFramebuffers(1, &drawFBO);
        fDeleteFramebuffers(1, &readFBO);
        fDeleteRenderbuffers(1, &colorMSRB);
        fDeleteRenderbuffers(1, &depthRB);
        fDeleteRenderbuffers(1, &stencilRB);
        fDeleteTextures(1, &texture);

        return false;
    }

    // Success, so switch everything out.
    // Store current user FBO bindings.
    GLuint boundDrawFBO = GetUserBoundDrawFBO();
    GLuint boundReadFBO = GetUserBoundReadFBO();

    // Replace with the new hotness
    std::swap(mOffscreenDrawFBO, drawFBO);
    std::swap(mOffscreenReadFBO, readFBO);
    std::swap(mOffscreenColorRB, colorMSRB);
    std::swap(mOffscreenDepthRB, depthRB);
    std::swap(mOffscreenStencilRB, stencilRB);
    std::swap(mOffscreenTexture, texture);

    // Delete the old and busted
    fDeleteFramebuffers(1, &drawFBO);
    fDeleteFramebuffers(1, &readFBO);
    fDeleteRenderbuffers(1, &colorMSRB);
    fDeleteRenderbuffers(1, &depthRB);
    fDeleteRenderbuffers(1, &stencilRB);
    fDeleteTextures(1, &texture);

    // Rebind user FBOs, in case anything changed internally.
    BindUserDrawFBO(boundDrawFBO);
    BindUserReadFBO(boundReadFBO);

    // Newly-created buffers are...unlikely to match.
    ForceDirtyFBOs();

    // Finish up.
    mOffscreenSize = aSize;
    mOffscreenActualSize = aSize;
    mActualFormat = cf;

    if (DebugMode()) {
        printf_stderr("Resized %dx%d offscreen FBO: r: %d g: %d b: %d a: %d depth: %d stencil: %d samples: %d\n",
                      mOffscreenActualSize.width, mOffscreenActualSize.height,
                      mActualFormat.red, mActualFormat.green, mActualFormat.blue, mActualFormat.alpha,
                      mActualFormat.depth, mActualFormat.stencil, mActualFormat.samples);
    }

    return true;
}

void
GLContext::DeleteOffscreenFBOs()
{
    fDeleteFramebuffers(1, &mOffscreenDrawFBO);
    fDeleteFramebuffers(1, &mOffscreenReadFBO);
    fDeleteTextures(1, &mOffscreenTexture);
    fDeleteRenderbuffers(1, &mOffscreenColorRB);
    fDeleteRenderbuffers(1, &mOffscreenDepthRB);
    fDeleteRenderbuffers(1, &mOffscreenStencilRB);

    mOffscreenDrawFBO = 0;
    mOffscreenReadFBO = 0;
    mOffscreenTexture = 0;
    mOffscreenColorRB = 0;
    mOffscreenDepthRB = 0;
    mOffscreenStencilRB = 0;
}

void
GLContext::ClearSafely()
{
    // bug 659349 --- we must be very careful here: clearing a GL framebuffer is nontrivial, relies on a lot of state,
    // and in the case of the backbuffer of a WebGL context, state is exposed to scripts.
    //
    // The code here is taken from WebGLContext::ForceClearFramebufferWithDefaultValues, but I didn't find a good way of
    // sharing code with it. WebGL's code is somewhat performance-critical as it is typically called on every frame, so
    // WebGL keeps track of GL state to avoid having to query it everytime, and also tries to only do work for actually
    // present buffers (e.g. stencil buffer). Doing that here seems like premature optimization,
    // as ClearSafely() is called only when e.g. a canvas is resized, not on every animation frame.

    realGLboolean scissorTestEnabled;
    realGLboolean ditherEnabled;
    realGLboolean colorWriteMask[4];
    realGLboolean depthWriteMask;
    GLint stencilWriteMaskFront, stencilWriteMaskBack;
    GLfloat colorClearValue[4];
    GLfloat depthClearValue;
    GLint stencilClearValue;

    // save current GL state
    fGetBooleanv(LOCAL_GL_SCISSOR_TEST, &scissorTestEnabled);
    fGetBooleanv(LOCAL_GL_DITHER, &ditherEnabled);
    fGetBooleanv(LOCAL_GL_COLOR_WRITEMASK, colorWriteMask);
    fGetBooleanv(LOCAL_GL_DEPTH_WRITEMASK, &depthWriteMask);
    fGetIntegerv(LOCAL_GL_STENCIL_WRITEMASK, &stencilWriteMaskFront);
    fGetIntegerv(LOCAL_GL_STENCIL_BACK_WRITEMASK, &stencilWriteMaskBack);
    fGetFloatv(LOCAL_GL_COLOR_CLEAR_VALUE, colorClearValue);
    fGetFloatv(LOCAL_GL_DEPTH_CLEAR_VALUE, &depthClearValue);
    fGetIntegerv(LOCAL_GL_STENCIL_CLEAR_VALUE, &stencilClearValue);

    // prepare GL state for clearing
    fDisable(LOCAL_GL_SCISSOR_TEST);
    fDisable(LOCAL_GL_DITHER);
    PushViewportRect(nsIntRect(0, 0, mOffscreenSize.width, mOffscreenSize.height));

    fColorMask(1, 1, 1, 1);
    fClearColor(0.f, 0.f, 0.f, 0.f);

    fDepthMask(1);
    fClearDepth(1.0f);

    fStencilMask(0xffffffff);
    fClearStencil(0);

    // do clear
    fClear(LOCAL_GL_COLOR_BUFFER_BIT |
           LOCAL_GL_DEPTH_BUFFER_BIT |
           LOCAL_GL_STENCIL_BUFFER_BIT);

    // restore GL state after clearing
    fColorMask(colorWriteMask[0],
               colorWriteMask[1],
               colorWriteMask[2],
               colorWriteMask[3]);
    fClearColor(colorClearValue[0],
                colorClearValue[1],
                colorClearValue[2],
                colorClearValue[3]);

    fDepthMask(depthWriteMask);
    fClearDepth(depthClearValue);

    fStencilMaskSeparate(LOCAL_GL_FRONT, stencilWriteMaskFront);
    fStencilMaskSeparate(LOCAL_GL_BACK, stencilWriteMaskBack);
    fClearStencil(stencilClearValue);

    PopViewportRect();

    if (ditherEnabled)
        fEnable(LOCAL_GL_DITHER);
    else
        fDisable(LOCAL_GL_DITHER);

    if (scissorTestEnabled)
        fEnable(LOCAL_GL_SCISSOR_TEST);
    else
        fDisable(LOCAL_GL_SCISSOR_TEST);

}

void
GLContext::UpdateActualFormat()
{
    ContextFormat nf;

    fGetIntegerv(LOCAL_GL_RED_BITS, (GLint*) &nf.red);
    fGetIntegerv(LOCAL_GL_GREEN_BITS, (GLint*) &nf.green);
    fGetIntegerv(LOCAL_GL_BLUE_BITS, (GLint*) &nf.blue);
    fGetIntegerv(LOCAL_GL_ALPHA_BITS, (GLint*) &nf.alpha);
    fGetIntegerv(LOCAL_GL_DEPTH_BITS, (GLint*) &nf.depth);
    fGetIntegerv(LOCAL_GL_STENCIL_BITS, (GLint*) &nf.stencil);

    mActualFormat = nf;
}

void
GLContext::MarkDestroyed()
{
    if (IsDestroyed())
        return;

    if (MakeCurrent()) {
        DeleteOffscreenFBOs();
        DeleteTexBlitProgram();

        fDeleteProgram(mBlitProgram);
        mBlitProgram = 0;
        fDeleteFramebuffers(1, &mBlitFramebuffer);
        mBlitFramebuffer = 0;
    } else {
        NS_WARNING("MakeCurrent() failed during MarkDestroyed! Skipping GL object teardown.");
    }

    mSymbols.Zero();
}

static void SwapRAndBComponents(gfxImageSurface* surf)
{
    for (int j = 0; j < surf->Height(); ++j) {
        uint32_t* row = (uint32_t*)(surf->Data() + surf->Stride() * j);
        for (int i = 0; i < surf->Width(); ++i) {
            *row = (*row & 0xff00ff00) | ((*row & 0xff) << 16) | ((*row & 0xff0000) >> 16);
            row++;
        }
    }
}

static already_AddRefed<gfxImageSurface> YInvertImageSurface(gfxImageSurface* aSurf)
{
  gfxIntSize size = aSurf->GetSize();
  nsRefPtr<gfxImageSurface> temp = new gfxImageSurface(size, aSurf->Format());
  nsRefPtr<gfxContext> ctx = new gfxContext(temp);
  ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx->Scale(1.0, -1.0);
  ctx->Translate(-gfxPoint(0.0, size.height));
  ctx->SetSource(aSurf);
  ctx->Paint();
  return temp.forget();
}

already_AddRefed<gfxImageSurface>
GLContext::GetTexImage(GLuint aTexture, bool aYInvert, ShaderProgramType aShader)
{
    MakeCurrent();
    GuaranteeResolve();
    fActiveTexture(LOCAL_GL_TEXTURE0);
    fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);

    gfxIntSize size;
    fGetTexLevelParameteriv(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_TEXTURE_WIDTH, &size.width);
    fGetTexLevelParameteriv(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_TEXTURE_HEIGHT, &size.height);
    
    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(size, gfxASurface::ImageFormatARGB32);
    if (!surf || surf->CairoStatus()) {
        return NULL;
    }

    uint32_t currentPackAlignment = 0;
    fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, (GLint*)&currentPackAlignment);
    if (currentPackAlignment != 4) {
        fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 4);
    }
    fGetTexImage(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, surf->Data());
    if (currentPackAlignment != 4) {
        fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, currentPackAlignment);
    }
   
    if (aShader == RGBALayerProgramType || aShader == RGBXLayerProgramType) {
      SwapRAndBComponents(surf);
    }

    if (aYInvert) {
      surf = YInvertImageSurface(surf);
    }
    return surf.forget();
}

already_AddRefed<gfxImageSurface>
GLContext::ReadTextureImage(GLuint aTexture,
                            const gfxIntSize& aSize,
                            GLenum aTextureFormat,
                            bool aYInvert)
{
    MakeCurrent();

    nsRefPtr<gfxImageSurface> isurf;

    GLint oldrb, oldfb, oldprog, oldPackAlignment;
    GLint success;

    GLuint rb = 0, fb = 0;
    GLuint vs = 0, fs = 0, prog = 0;

    const char *vShader =
        "attribute vec4 aVertex;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 vTexCoord;\n"
        "void main() { gl_Position = aVertex; vTexCoord = aTexCoord; }";
    const char *fShader =
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
        "varying vec2 vTexCoord;\n"
        "uniform sampler2D uTexture;\n"
        "void main() { gl_FragColor = texture2D(uTexture, vTexCoord); }";

    float verts[4*4] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f
    };

    float texcoords[2*4] = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
    };

    fGetIntegerv(LOCAL_GL_RENDERBUFFER_BINDING, &oldrb);
    fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &oldfb);
    fGetIntegerv(LOCAL_GL_CURRENT_PROGRAM, &oldprog);
    fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, &oldPackAlignment);

    PushViewportRect(nsIntRect(0, 0, aSize.width, aSize.height));

    fGenRenderbuffers(1, &rb);
    fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, rb);
    fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, LOCAL_GL_RGBA,
                         aSize.width, aSize.height);

    fGenFramebuffers(1, &fb);
    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fb);
    fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                             LOCAL_GL_RENDERBUFFER, rb);

    if (fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) !=
        LOCAL_GL_FRAMEBUFFER_COMPLETE)
    {
        goto cleanup;
    }

    vs = fCreateShader(LOCAL_GL_VERTEX_SHADER);
    fs = fCreateShader(LOCAL_GL_FRAGMENT_SHADER);
    fShaderSource(vs, 1, (const GLchar**) &vShader, NULL);
    fShaderSource(fs, 1, (const GLchar**) &fShader, NULL);
    fCompileShader(vs);
    fCompileShader(fs);
    prog = fCreateProgram();
    fAttachShader(prog, vs);
    fAttachShader(prog, fs);
    fBindAttribLocation(prog, 0, "aVertex");
    fBindAttribLocation(prog, 1, "aTexCoord");
    fLinkProgram(prog);

    fGetProgramiv(prog, LOCAL_GL_LINK_STATUS, &success);
    if (!success) {
        goto cleanup;
    }

    fUseProgram(prog);

    fEnableVertexAttribArray(0);
    fEnableVertexAttribArray(1);

    fVertexAttribPointer(0, 4, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, verts);
    fVertexAttribPointer(1, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, texcoords);

    fActiveTexture(LOCAL_GL_TEXTURE0);
    fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);

    fUniform1i(fGetUniformLocation(prog, "uTexture"), 0);

    fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);

    fDisableVertexAttribArray(1);
    fDisableVertexAttribArray(0);

    isurf = new gfxImageSurface(aSize, gfxASurface::ImageFormatARGB32);
    if (!isurf || isurf->CairoStatus()) {
        isurf = nullptr;
        goto cleanup;
    }

    if (oldPackAlignment != 4)
        fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 4);

    fReadPixels(0, 0, aSize.width, aSize.height,
                LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                isurf->Data());

    SwapRAndBComponents(isurf);

    if (oldPackAlignment != 4)
        fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, oldPackAlignment);

    if (aYInvert) {
      isurf = YInvertImageSurface(isurf);
    }

 cleanup:
    // note that deleting 0 has no effect in any of these calls
    fDeleteRenderbuffers(1, &rb);
    fDeleteFramebuffers(1, &fb);
    fDeleteShader(vs);
    fDeleteShader(fs);
    fDeleteProgram(prog);

    fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, oldrb);
    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, oldfb);
    fUseProgram(oldprog);

    PopViewportRect();

    return isurf.forget();
}

static void
GetOptimalReadFormats(GLContext* gl, GLenum& format, GLenum& type) {
    if (gl->IsGLES2()) {
        bool has_BGRA_UByte = false;
        if (gl->IsExtensionSupported(gl::GLContext::EXT_bgra)) {
          has_BGRA_UByte = true;
        } else if (gl->IsExtensionSupported(gl::GLContext::EXT_read_format_bgra) ||
                   gl->IsExtensionSupported(gl::GLContext::IMG_read_format)) {
            // Note that these extensions are not required to query this value.
            // However, we should never get back BGRA unless one of these is supported.
            GLint auxFormat = 0;
            GLint auxType = 0;

            gl->fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT, &auxFormat);
            gl->fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_TYPE, &auxType);

            if (auxFormat == LOCAL_GL_BGRA && auxType == LOCAL_GL_UNSIGNED_BYTE)
              has_BGRA_UByte = true;
        }

        format = has_BGRA_UByte ? LOCAL_GL_BGRA : LOCAL_GL_RGBA;
        type = LOCAL_GL_UNSIGNED_BYTE;
    } else {
        // defaults for desktop
        format = LOCAL_GL_BGRA;
        type = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
    }
}

void
GLContext::ReadScreenIntoImageSurface(gfxImageSurface* dest)
{
    GLuint boundFB = 0;
    fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, (GLint*)&boundFB);
    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

    ReadPixelsIntoImageSurface(dest);

    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, boundFB);
}

void
GLContext::ReadPixelsIntoImageSurface(gfxImageSurface* dest)
{
    MOZ_ASSERT(dest->Format() == gfxASurface::ImageFormatARGB32 ||
               dest->Format() == gfxASurface::ImageFormatRGB24);

    MOZ_ASSERT(dest->Stride() == dest->Width() * 4);
    MOZ_ASSERT(dest->Format() == gfxASurface::ImageFormatARGB32 ||
               dest->Format() == gfxASurface::ImageFormatRGB24);

    MOZ_ASSERT(dest->Stride() == dest->Width() * 4);

    MakeCurrent();

    GLint currentPackAlignment = 0;
    fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, &currentPackAlignment);

    if (currentPackAlignment != 4)
        fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 4);

    GLenum format;
    GLenum datatype;
    GetOptimalReadFormats(this, format, datatype);

    GLsizei width = dest->Width();
    GLsizei height = dest->Height();

    fReadPixels(0, 0,
                width, height,
                format, datatype,
                dest->Data());

    // Check if GL is giving back 1.0 alpha for
    // RGBA reads to RGBA images from no-alpha buffers.
#ifdef XP_MACOSX
    if (WorkAroundDriverBugs() &&
        mVendor == VendorNVIDIA &&
        dest->Format() == gfxASurface::ImageFormatARGB32 &&
        width && height)
    {
        GLint alphaBits = 0;
        fGetIntegerv(LOCAL_GL_ALPHA_BITS, &alphaBits);
        if (!alphaBits) {
            const uint32_t alphaMask = gfxPackedPixelNoPreMultiply(0xff,0,0,0);

            uint32_t* itr = (uint32_t*)dest->Data();
            uint32_t testPixel = *itr;
            if ((testPixel & alphaMask) != alphaMask) {
                // We need to set the alpha channel to 1.0 manually.
                uint32_t* itrEnd = itr + width*height;  // Stride is guaranteed to be width*4.

                for (; itr != itrEnd; itr++) {
                    *itr |= alphaMask;
                }
            }
        }
    }
#endif

    // Output should be in BGRA, so swap if RGBA.
    if (format == LOCAL_GL_RGBA) {
        SwapRAndBComponents(dest);
    }

    if (currentPackAlignment != 4)
        fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, currentPackAlignment);
}

void
GLContext::BlitTextureImage(TextureImage *aSrc, const nsIntRect& aSrcRect,
                            TextureImage *aDst, const nsIntRect& aDstRect)
{
    NS_ASSERTION(!aSrc->InUpdate(), "Source texture is in update!");
    NS_ASSERTION(!aDst->InUpdate(), "Destination texture is in update!");

    if (aSrcRect.IsEmpty() || aDstRect.IsEmpty())
        return;

    // only save/restore this stuff on Qualcomm Adreno, to work
    // around an apparent bug
    int savedFb = 0;
    if (mWorkAroundDriverBugs &&
        mVendor == VendorQualcomm)
    {
        fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &savedFb);
    }

    fDisable(LOCAL_GL_SCISSOR_TEST);
    fDisable(LOCAL_GL_BLEND);

    // 2.0 means scale up by two
    float blitScaleX = float(aDstRect.width) / float(aSrcRect.width);
    float blitScaleY = float(aDstRect.height) / float(aSrcRect.height);

    // We start iterating over all destination tiles
    aDst->BeginTileIteration();
    do {
        // calculate portion of the tile that is going to be painted to
        nsIntRect dstSubRect;
        nsIntRect dstTextureRect = aDst->GetTileRect();
        dstSubRect.IntersectRect(aDstRect, dstTextureRect);

        // this tile is not part of the destination rectangle aDstRect
        if (dstSubRect.IsEmpty())
            continue;

        // (*) transform the rect of this tile into the rectangle defined by aSrcRect...
        nsIntRect dstInSrcRect(dstSubRect);
        dstInSrcRect.MoveBy(-aDstRect.TopLeft());
        // ...which might be of different size, hence scale accordingly
        dstInSrcRect.ScaleRoundOut(1.0f / blitScaleX, 1.0f / blitScaleY);
        dstInSrcRect.MoveBy(aSrcRect.TopLeft());

        SetBlitFramebufferForDestTexture(aDst->GetTextureID());
        UseBlitProgram();

        aSrc->BeginTileIteration();
        // now iterate over all tiles in the source Image...
        do {
            // calculate portion of the source tile that is in the source rect
            nsIntRect srcSubRect;
            nsIntRect srcTextureRect = aSrc->GetTileRect();
            srcSubRect.IntersectRect(aSrcRect, srcTextureRect);

            // this tile is not part of the source rect
            if (srcSubRect.IsEmpty()) {
                continue;
            }
            // calculate intersection of source rect with destination rect
            srcSubRect.IntersectRect(srcSubRect, dstInSrcRect);
            // this tile does not overlap the current destination tile
            if (srcSubRect.IsEmpty()) {
                continue;
            }
            // We now have the intersection of 
            //     the current source tile 
            // and the desired source rectangle
            // and the destination tile
            // and the desired destination rectange
            // in destination space.
            // We need to transform this back into destination space, inverting the transform from (*)
            nsIntRect srcSubInDstRect(srcSubRect);
            srcSubInDstRect.MoveBy(-aSrcRect.TopLeft());
            srcSubInDstRect.ScaleRoundOut(blitScaleX, blitScaleY);
            srcSubInDstRect.MoveBy(aDstRect.TopLeft());

            // we transform these rectangles to be relative to the current src and dst tiles, respectively
            nsIntSize srcSize = srcTextureRect.Size();
            nsIntSize dstSize = dstTextureRect.Size();
            srcSubRect.MoveBy(-srcTextureRect.x, -srcTextureRect.y);
            srcSubInDstRect.MoveBy(-dstTextureRect.x, -dstTextureRect.y);

            float dx0 = 2.0 * float(srcSubInDstRect.x) / float(dstSize.width) - 1.0;
            float dy0 = 2.0 * float(srcSubInDstRect.y) / float(dstSize.height) - 1.0;
            float dx1 = 2.0 * float(srcSubInDstRect.x + srcSubInDstRect.width) / float(dstSize.width) - 1.0;
            float dy1 = 2.0 * float(srcSubInDstRect.y + srcSubInDstRect.height) / float(dstSize.height) - 1.0;
            PushViewportRect(nsIntRect(0, 0, dstSize.width, dstSize.height));

            RectTriangles rects;

            nsIntSize realTexSize = srcSize;
            if (!CanUploadNonPowerOfTwo()) {
                realTexSize = nsIntSize(NextPowerOfTwo(srcSize.width),
                                        NextPowerOfTwo(srcSize.height));
            }

            if (aSrc->GetWrapMode() == LOCAL_GL_REPEAT) {
                rects.addRect(/* dest rectangle */
                        dx0, dy0, dx1, dy1,
                        /* tex coords */
                        srcSubRect.x / float(realTexSize.width),
                        srcSubRect.y / float(realTexSize.height),
                        srcSubRect.XMost() / float(realTexSize.width),
                        srcSubRect.YMost() / float(realTexSize.height));
            } else {
                DecomposeIntoNoRepeatTriangles(srcSubRect, realTexSize, rects);

                // now put the coords into the d[xy]0 .. d[xy]1 coordinate space
                // from the 0..1 that it comes out of decompose
                RectTriangles::vert_coord* v = (RectTriangles::vert_coord*)rects.vertexPointer();

                for (unsigned int i = 0; i < rects.elements(); ++i) {
                    v[i].x = (v[i].x * (dx1 - dx0)) + dx0;
                    v[i].y = (v[i].y * (dy1 - dy0)) + dy0;
                }
            }

            TextureImage::ScopedBindTexture texBind(aSrc, LOCAL_GL_TEXTURE0);

            fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

            fVertexAttribPointer(0, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, rects.vertexPointer());
            fVertexAttribPointer(1, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, rects.texCoordPointer());

            fEnableVertexAttribArray(0);
            fEnableVertexAttribArray(1);

            fDrawArrays(LOCAL_GL_TRIANGLES, 0, rects.elements());

            fDisableVertexAttribArray(0);
            fDisableVertexAttribArray(1);

            PopViewportRect();
        } while (aSrc->NextTile());
    } while (aDst->NextTile());

    fVertexAttribPointer(0, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, NULL);
    fVertexAttribPointer(1, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, NULL);

    // unbind the previous texture from the framebuffer
    SetBlitFramebufferForDestTexture(0);

    // then put back the previous framebuffer, and don't
    // enable stencil if it wasn't enabled on entry to work
    // around Adreno 200 bug that causes us to crash if
    // we enable scissor test while the current FBO is invalid
    // (which it will be, once we assign texture 0 to the color
    // attachment)
    if (mWorkAroundDriverBugs &&
        mVendor == VendorQualcomm) {
        fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, savedFb);
    }

    fEnable(LOCAL_GL_SCISSOR_TEST);
    fEnable(LOCAL_GL_BLEND);
}

static unsigned int 
DataOffset(gfxImageSurface *aSurf, const nsIntPoint &aPoint)
{
  unsigned int data = aPoint.y * aSurf->Stride();
  data += aPoint.x * gfxASurface::BytePerPixelFromFormat(aSurf->Format());
  return data;
}

ShaderProgramType 
GLContext::UploadSurfaceToTexture(gfxASurface *aSurface, 
                                  const nsIntRegion& aDstRegion,
                                  GLuint& aTexture,
                                  bool aOverwrite,
                                  const nsIntPoint& aSrcPoint,
                                  bool aPixelBuffer,
                                  GLenum aTextureUnit)
{
    bool textureInited = aOverwrite ? false : true;
    MakeCurrent();
    fActiveTexture(aTextureUnit);
  
    if (!aTexture) {
        fGenTextures(1, &aTexture);
        fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, 
                       LOCAL_GL_TEXTURE_MIN_FILTER, 
                       LOCAL_GL_LINEAR);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, 
                       LOCAL_GL_TEXTURE_MAG_FILTER, 
                       LOCAL_GL_LINEAR);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, 
                       LOCAL_GL_TEXTURE_WRAP_S, 
                       LOCAL_GL_CLAMP_TO_EDGE);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, 
                       LOCAL_GL_TEXTURE_WRAP_T, 
                       LOCAL_GL_CLAMP_TO_EDGE);
        textureInited = false;
    } else {
        fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);
    }

    nsIntRegion paintRegion;
    if (!textureInited) {
        paintRegion = nsIntRegion(aDstRegion.GetBounds());
    } else {
        paintRegion = aDstRegion;
    }

    nsRefPtr<gfxImageSurface> imageSurface = aSurface->GetAsImageSurface();
    unsigned char* data = NULL;

    if (!imageSurface || 
        (imageSurface->Format() != gfxASurface::ImageFormatARGB32 &&
         imageSurface->Format() != gfxASurface::ImageFormatRGB24 &&
         imageSurface->Format() != gfxASurface::ImageFormatRGB16_565 &&
         imageSurface->Format() != gfxASurface::ImageFormatA8)) {
        // We can't get suitable pixel data for the surface, make a copy
        nsIntRect bounds = aDstRegion.GetBounds();
        imageSurface = 
          new gfxImageSurface(gfxIntSize(bounds.width, bounds.height), 
                              gfxASurface::ImageFormatARGB32);
  
        nsRefPtr<gfxContext> context = new gfxContext(imageSurface);

        context->Translate(-gfxPoint(aSrcPoint.x, aSrcPoint.y));
        context->SetSource(aSurface);
        context->Paint();
        data = imageSurface->Data();
        NS_ASSERTION(!aPixelBuffer,
                     "Must be using an image compatible surface with pixel buffers!");
    } else {
        // If a pixel buffer is bound the data pointer parameter is relative
        // to the start of the data block.
        if (!aPixelBuffer) {
              data = imageSurface->Data();
        }
        data += DataOffset(imageSurface, aSrcPoint);
    }

    GLenum format;
    GLenum type;
    int32_t pixelSize = gfxASurface::BytePerPixelFromFormat(imageSurface->Format());
    ShaderProgramType shader;

    switch (imageSurface->Format()) {
        case gfxASurface::ImageFormatARGB32:
            format = LOCAL_GL_RGBA;
            type = LOCAL_GL_UNSIGNED_BYTE;
            shader = BGRALayerProgramType;
            break;
        case gfxASurface::ImageFormatRGB24:
            // Treat RGB24 surfaces as RGBA32 except for the shader
            // program used.
            format = LOCAL_GL_RGBA;
            type = LOCAL_GL_UNSIGNED_BYTE;
            shader = BGRXLayerProgramType;
            break;
        case gfxASurface::ImageFormatRGB16_565:
            format = LOCAL_GL_RGB;
            type = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
            shader = RGBALayerProgramType;
            break;
        case gfxASurface::ImageFormatA8:
            format = LOCAL_GL_LUMINANCE;
            type = LOCAL_GL_UNSIGNED_BYTE;
            // We don't have a specific luminance shader
            shader = ShaderProgramType(0);
            break;
        default:
            NS_ASSERTION(false, "Unhandled image surface format!");
            format = 0;
            type = 0;
            shader = ShaderProgramType(0);
    }

    int32_t stride = imageSurface->Stride();

    nsIntRegionRectIterator iter(paintRegion);
    const nsIntRect *iterRect;

    // Top left point of the region's bounding rectangle.
    nsIntPoint topLeft = paintRegion.GetBounds().TopLeft();

    while ((iterRect = iter.Next())) {
        // The inital data pointer is at the top left point of the region's
        // bounding rectangle. We need to find the offset of this rect
        // within the region and adjust the data pointer accordingly.
        unsigned char *rectData = 
            data + DataOffset(imageSurface, iterRect->TopLeft() - topLeft);

        NS_ASSERTION(textureInited || (iterRect->x == 0 && iterRect->y == 0), 
                     "Must be uploading to the origin when we don't have an existing texture");

        if (textureInited && CanUploadSubTextures()) {
            TexSubImage2D(LOCAL_GL_TEXTURE_2D,
                          0,
                          iterRect->x,
                          iterRect->y,
                          iterRect->width,
                          iterRect->height,
                          stride,
                          pixelSize,
                          format,
                          type,
                          rectData);
        } else {
            TexImage2D(LOCAL_GL_TEXTURE_2D,
                       0,
                       format,
                       iterRect->width,
                       iterRect->height,
                       stride,
                       pixelSize,
                       0,
                       format,
                       type,
                       rectData);
        }

    }

    return shader;
}

static GLint GetAddressAlignment(ptrdiff_t aAddress)
{
    if (!(aAddress & 0x7)) {
       return 8;
    } else if (!(aAddress & 0x3)) {
        return 4;
    } else if (!(aAddress & 0x1)) {
        return 2;
    } else {
        return 1;
    }
}

void
GLContext::TexImage2D(GLenum target, GLint level, GLint internalformat,
                      GLsizei width, GLsizei height, GLsizei stride,
                      GLint pixelsize, GLint border, GLenum format,
                      GLenum type, const GLvoid *pixels)
{
    if (mIsGLES2) {

        NS_ASSERTION(format == (GLenum)internalformat,
                    "format and internalformat not the same for glTexImage2D on GLES2");

        if (!CanUploadNonPowerOfTwo()
            && (stride != width * pixelsize
            || !IsPowerOfTwo(width)
            || !IsPowerOfTwo(height))) {

            // Pad out texture width and height to the next power of two
            // as we don't support/want non power of two texture uploads
            GLsizei paddedWidth = NextPowerOfTwo(width);
            GLsizei paddedHeight = NextPowerOfTwo(height);

            GLvoid* paddedPixels = new unsigned char[paddedWidth * paddedHeight * pixelsize];

            // Pad out texture data to be in a POT sized buffer for uploading to
            // a POT sized texture
            CopyAndPadTextureData(pixels, paddedPixels, width, height,
                                  paddedWidth, paddedHeight, stride, pixelsize);

            fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                    NS_MIN(GetAddressAlignment((ptrdiff_t)paddedPixels),
                            GetAddressAlignment((ptrdiff_t)paddedWidth * pixelsize)));
            fTexImage2D(target,
                        border,
                        internalformat,
                        paddedWidth,
                        paddedHeight,
                        border,
                        format,
                        type,
                        paddedPixels);
            fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);

            delete[] static_cast<unsigned char*>(paddedPixels);
            return;
        }

        if (stride == width * pixelsize) {
            fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                    NS_MIN(GetAddressAlignment((ptrdiff_t)pixels),
                            GetAddressAlignment((ptrdiff_t)stride)));
            fTexImage2D(target,
                        border,
                        internalformat,
                        width,
                        height,
                        border,
                        format,
                        type,
                        pixels);
            fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
        } else {
            // Use GLES-specific workarounds for GL_UNPACK_ROW_LENGTH; these are
            // implemented in TexSubImage2D.
            fTexImage2D(target,
                        border,
                        internalformat,
                        width,
                        height,
                        border,
                        format,
                        type,
                        NULL);
            TexSubImage2D(target,
                          level,
                          0,
                          0,
                          width,
                          height,
                          stride,
                          pixelsize,
                          format,
                          type,
                          pixels);
        }
    } else {
        // desktop GL (non-ES) path

        fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                    NS_MIN(GetAddressAlignment((ptrdiff_t)pixels),
                            GetAddressAlignment((ptrdiff_t)stride)));
        int rowLength = stride/pixelsize;
        fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
        fTexImage2D(target,
                    level,
                    internalformat,
                    width,
                    height,
                    border,
                    format,
                    type,
                    pixels);
        fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
        fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
    }
}

void
GLContext::TexSubImage2D(GLenum target, GLint level,
                         GLint xoffset, GLint yoffset,
                         GLsizei width, GLsizei height, GLsizei stride,
                         GLint pixelsize, GLenum format,
                         GLenum type, const GLvoid* pixels)
{
    if (mIsGLES2) {
        if (stride == width * pixelsize) {
            fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                    NS_MIN(GetAddressAlignment((ptrdiff_t)pixels),
                            GetAddressAlignment((ptrdiff_t)stride)));
            fTexSubImage2D(target,
                          level,
                          xoffset,
                          yoffset,
                          width,
                          height,
                          format,
                          type,
                          pixels);
            fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
        } else if (IsExtensionSupported(EXT_unpack_subimage)) {
            TexSubImage2DWithUnpackSubimageGLES(target, level, xoffset, yoffset,
                                                width, height, stride,
                                                pixelsize, format, type, pixels);

        } else {
            TexSubImage2DWithoutUnpackSubimage(target, level, xoffset, yoffset,
                                              width, height, stride,
                                              pixelsize, format, type, pixels);
        }
    } else {
        // desktop GL (non-ES) path
        fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                    NS_MIN(GetAddressAlignment((ptrdiff_t)pixels),
                            GetAddressAlignment((ptrdiff_t)stride)));
        int rowLength = stride/pixelsize;
        fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
        fTexSubImage2D(target,
                      level,
                      xoffset,
                      yoffset,
                      width,
                      height,
                      format,
                      type,
                      pixels);
        fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
        fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
    }
}

void
GLContext::TexSubImage2DWithUnpackSubimageGLES(GLenum target, GLint level,
                                               GLint xoffset, GLint yoffset,
                                               GLsizei width, GLsizei height,
                                               GLsizei stride, GLint pixelsize,
                                               GLenum format, GLenum type,
                                               const GLvoid* pixels)
{
    fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                 NS_MIN(GetAddressAlignment((ptrdiff_t)pixels),
                        GetAddressAlignment((ptrdiff_t)stride)));
    // When using GL_UNPACK_ROW_LENGTH, we need to work around a Tegra
    // driver crash where the driver apparently tries to read
    // (stride - width * pixelsize) bytes past the end of the last input
    // row. We only upload the first height-1 rows using GL_UNPACK_ROW_LENGTH,
    // and then we upload the final row separately. See bug 697990.
    int rowLength = stride/pixelsize;
    fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
    fTexSubImage2D(target,
                    level,
                    xoffset,
                    yoffset,
                    width,
                    height-1,
                    format,
                    type,
                    pixels);
    fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
    fTexSubImage2D(target,
                    level,
                    xoffset,
                    yoffset+height-1,
                    width,
                    1,
                    format,
                    type,
                    (const unsigned char *)pixels+(height-1)*stride);
    fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
}

void
GLContext::TexSubImage2DWithoutUnpackSubimage(GLenum target, GLint level,
                                              GLint xoffset, GLint yoffset,
                                              GLsizei width, GLsizei height,
                                              GLsizei stride, GLint pixelsize,
                                              GLenum format, GLenum type,
                                              const GLvoid* pixels)
{
    // Not using the whole row of texture data and GL_UNPACK_ROW_LENGTH
    // isn't supported. We make a copy of the texture data we're using,
    // such that we're using the whole row of data in the copy. This turns
    // out to be more efficient than uploading row-by-row; see bug 698197.
    unsigned char *newPixels = new unsigned char[width*height*pixelsize];
    unsigned char *rowDest = newPixels;
    const unsigned char *rowSource = (const unsigned char *)pixels;
    for (int h = 0; h < height; h++) {
            memcpy(rowDest, rowSource, width*pixelsize);
            rowDest += width*pixelsize;
            rowSource += stride;
    }

    stride = width*pixelsize;
    fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                    NS_MIN(GetAddressAlignment((ptrdiff_t)newPixels),
                            GetAddressAlignment((ptrdiff_t)stride)));
    fTexSubImage2D(target,
                    level,
                    xoffset,
                    yoffset,
                    width,
                    height,
                    format,
                    type,
                    newPixels);
    delete [] newPixels;
    fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
}

void
GLContext::RectTriangles::addRect(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1,
                                  GLfloat tx0, GLfloat ty0, GLfloat tx1, GLfloat ty1,
                                  bool flip_y /* = false */)
{
    vert_coord v;
    v.x = x0; v.y = y0;
    vertexCoords.AppendElement(v);
    v.x = x1; v.y = y0;
    vertexCoords.AppendElement(v);
    v.x = x0; v.y = y1;
    vertexCoords.AppendElement(v);

    v.x = x0; v.y = y1;
    vertexCoords.AppendElement(v);
    v.x = x1; v.y = y0;
    vertexCoords.AppendElement(v);
    v.x = x1; v.y = y1;
    vertexCoords.AppendElement(v);

    if (flip_y) {
        tex_coord t;
        t.u = tx0; t.v = ty1;
        texCoords.AppendElement(t);
        t.u = tx1; t.v = ty1;
        texCoords.AppendElement(t);
        t.u = tx0; t.v = ty0;
        texCoords.AppendElement(t);

        t.u = tx0; t.v = ty0;
        texCoords.AppendElement(t);
        t.u = tx1; t.v = ty1;
        texCoords.AppendElement(t);
        t.u = tx1; t.v = ty0;
        texCoords.AppendElement(t);
    } else {
        tex_coord t;
        t.u = tx0; t.v = ty0;
        texCoords.AppendElement(t);
        t.u = tx1; t.v = ty0;
        texCoords.AppendElement(t);
        t.u = tx0; t.v = ty1;
        texCoords.AppendElement(t);

        t.u = tx0; t.v = ty1;
        texCoords.AppendElement(t);
        t.u = tx1; t.v = ty0;
        texCoords.AppendElement(t);
        t.u = tx1; t.v = ty1;
        texCoords.AppendElement(t);
    }
}

static GLfloat
WrapTexCoord(GLfloat v)
{
    // fmodf gives negative results for negative numbers;
    // that is, fmodf(0.75, 1.0) == 0.75, but
    // fmodf(-0.75, 1.0) == -0.75.  For the negative case,
    // the result we need is 0.25, so we add 1.0f.
    if (v < 0.0f) {
        return 1.0f + fmodf(v, 1.0f);
    }

    return fmodf(v, 1.0f);
}

void
GLContext::DecomposeIntoNoRepeatTriangles(const nsIntRect& aTexCoordRect,
                                          const nsIntSize& aTexSize,
                                          RectTriangles& aRects,
                                          bool aFlipY /* = false */)
{
    // normalize this
    nsIntRect tcr(aTexCoordRect);
    while (tcr.x >= aTexSize.width)
        tcr.x -= aTexSize.width;
    while (tcr.y >= aTexSize.height)
        tcr.y -= aTexSize.height;

    // Compute top left and bottom right tex coordinates
    GLfloat tl[2] =
        { GLfloat(tcr.x) / GLfloat(aTexSize.width),
          GLfloat(tcr.y) / GLfloat(aTexSize.height) };
    GLfloat br[2] =
        { GLfloat(tcr.XMost()) / GLfloat(aTexSize.width),
          GLfloat(tcr.YMost()) / GLfloat(aTexSize.height) };

    // then check if we wrap in either the x or y axis; if we do,
    // then also use fmod to figure out the "true" non-wrapping
    // texture coordinates.

    bool xwrap = false, ywrap = false;
    if (tcr.x < 0 || tcr.x > aTexSize.width ||
        tcr.XMost() < 0 || tcr.XMost() > aTexSize.width)
    {
        xwrap = true;
        tl[0] = WrapTexCoord(tl[0]);
        br[0] = WrapTexCoord(br[0]);
    }

    if (tcr.y < 0 || tcr.y > aTexSize.height ||
        tcr.YMost() < 0 || tcr.YMost() > aTexSize.height)
    {
        ywrap = true;
        tl[1] = WrapTexCoord(tl[1]);
        br[1] = WrapTexCoord(br[1]);
    }

    NS_ASSERTION(tl[0] >= 0.0f && tl[0] <= 1.0f &&
                 tl[1] >= 0.0f && tl[1] <= 1.0f &&
                 br[0] >= 0.0f && br[0] <= 1.0f &&
                 br[1] >= 0.0f && br[1] <= 1.0f,
                 "Somehow generated invalid texture coordinates");

    // If xwrap is false, the texture will be sampled from tl[0]
    // .. br[0].  If xwrap is true, then it will be split into tl[0]
    // .. 1.0, and 0.0 .. br[0].  Same for the Y axis.  The
    // destination rectangle is also split appropriately, according
    // to the calculated xmid/ymid values.

    // There isn't a 1:1 mapping between tex coords and destination coords;
    // when computing midpoints, we have to take that into account.  We
    // need to map the texture coords, which are (in the wrap case):
    // |tl->1| and |0->br| to the |0->1| range of the vertex coords.  So
    // we have the length (1-tl)+(br) that needs to map into 0->1.
    // These are only valid if there is wrap involved, they won't be used
    // otherwise.
    GLfloat xlen = (1.0f - tl[0]) + br[0];
    GLfloat ylen = (1.0f - tl[1]) + br[1];

    NS_ASSERTION(!xwrap || xlen > 0.0f, "xlen isn't > 0, what's going on?");
    NS_ASSERTION(!ywrap || ylen > 0.0f, "ylen isn't > 0, what's going on?");
    NS_ASSERTION(aTexCoordRect.width <= aTexSize.width &&
                 aTexCoordRect.height <= aTexSize.height, "tex coord rect would cause tiling!");

    if (!xwrap && !ywrap) {
        aRects.addRect(0.0f, 0.0f,
                       1.0f, 1.0f,
                       tl[0], tl[1],
                       br[0], br[1],
                       aFlipY);
    } else if (!xwrap && ywrap) {
        GLfloat ymid = (1.0f - tl[1]) / ylen;
        aRects.addRect(0.0f, 0.0f,
                       1.0f, ymid,
                       tl[0], tl[1],
                       br[0], 1.0f,
                       aFlipY);
        aRects.addRect(0.0f, ymid,
                       1.0f, 1.0f,
                       tl[0], 0.0f,
                       br[0], br[1],
                       aFlipY);
    } else if (xwrap && !ywrap) {
        GLfloat xmid = (1.0f - tl[0]) / xlen;
        aRects.addRect(0.0f, 0.0f,
                       xmid, 1.0f,
                       tl[0], tl[1],
                       1.0f, br[1],
                       aFlipY);
        aRects.addRect(xmid, 0.0f,
                       1.0f, 1.0f,
                       0.0f, tl[1],
                       br[0], br[1],
                       aFlipY);
    } else {
        GLfloat xmid = (1.0f - tl[0]) / xlen;
        GLfloat ymid = (1.0f - tl[1]) / ylen;
        aRects.addRect(0.0f, 0.0f,
                       xmid, ymid,
                       tl[0], tl[1],
                       1.0f, 1.0f,
                       aFlipY);
        aRects.addRect(xmid, 0.0f,
                       1.0f, ymid,
                       0.0f, tl[1],
                       br[0], 1.0f,
                       aFlipY);
        aRects.addRect(0.0f, ymid,
                       xmid, 1.0f,
                       tl[0], 0.0f,
                       1.0f, br[1],
                       aFlipY);
        aRects.addRect(xmid, ymid,
                       1.0f, 1.0f,
                       0.0f, 0.0f,
                       br[0], br[1],
                       aFlipY);
    }
}

void
GLContext::UseBlitProgram()
{
    if (mBlitProgram) {
        fUseProgram(mBlitProgram);
        return;
    }

    mBlitProgram = fCreateProgram();

    GLuint shaders[2];
    shaders[0] = fCreateShader(LOCAL_GL_VERTEX_SHADER);
    shaders[1] = fCreateShader(LOCAL_GL_FRAGMENT_SHADER);

    const char *blitVSSrc = 
        "attribute vec2 aVertex;"
        "attribute vec2 aTexCoord;"
        "varying vec2 vTexCoord;"
        "void main() {"
        "  vTexCoord = aTexCoord;"
        "  gl_Position = vec4(aVertex, 0.0, 1.0);"
        "}";
    const char *blitFSSrc = "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
        "uniform sampler2D uSrcTexture;"
        "varying vec2 vTexCoord;"
        "void main() {"
        "  gl_FragColor = texture2D(uSrcTexture, vTexCoord);"
        "}";

    fShaderSource(shaders[0], 1, (const GLchar**) &blitVSSrc, NULL);
    fShaderSource(shaders[1], 1, (const GLchar**) &blitFSSrc, NULL);

    for (int i = 0; i < 2; ++i) {
        GLint success, len = 0;

        fCompileShader(shaders[i]);
        fGetShaderiv(shaders[i], LOCAL_GL_COMPILE_STATUS, &success);
        NS_ASSERTION(success, "Shader compilation failed!");

        if (!success) {
            nsAutoCString log;
            fGetShaderiv(shaders[i], LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &len);
            log.SetCapacity(len);
            fGetShaderInfoLog(shaders[i], len, (GLint*) &len, (char*) log.BeginWriting());
            log.SetLength(len);

            printf_stderr("Shader %d compilation failed:\n%s\n", log.get());
            return;
        }

        fAttachShader(mBlitProgram, shaders[i]);
        fDeleteShader(shaders[i]);
    }

    fBindAttribLocation(mBlitProgram, 0, "aVertex");
    fBindAttribLocation(mBlitProgram, 1, "aTexCoord");

    fLinkProgram(mBlitProgram);

    GLint success, len = 0;
    fGetProgramiv(mBlitProgram, LOCAL_GL_LINK_STATUS, &success);
    NS_ASSERTION(success, "Shader linking failed!");

    if (!success) {
        nsAutoCString log;
        fGetProgramiv(mBlitProgram, LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &len);
        log.SetCapacity(len);
        fGetProgramInfoLog(mBlitProgram, len, (GLint*) &len, (char*) log.BeginWriting());
        log.SetLength(len);

        printf_stderr("Program linking failed:\n%s\n", log.get());
        return;
    }

    fUseProgram(mBlitProgram);
    fUniform1i(fGetUniformLocation(mBlitProgram, "uSrcTexture"), 0);
}

void
GLContext::SetBlitFramebufferForDestTexture(GLuint aTexture)
{
    if (!mBlitFramebuffer) {
        fGenFramebuffers(1, &mBlitFramebuffer);
    }

    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBlitFramebuffer);
    fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                          LOCAL_GL_COLOR_ATTACHMENT0,
                          LOCAL_GL_TEXTURE_2D,
                          aTexture,
                          0);

    GLenum result = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (aTexture && (result != LOCAL_GL_FRAMEBUFFER_COMPLETE)) {
        nsAutoCString msg;
        msg.Append("Framebuffer not complete -- error 0x");
        msg.AppendInt(result, 16);
        // Note: if you are hitting this, it is likely that
        // your texture is not texture complete -- that is, you
        // allocated a texture name, but didn't actually define its
        // size via a call to TexImage2D.
        NS_RUNTIMEABORT(msg.get());
    }
}

#ifdef DEBUG

void
GLContext::CreatedProgram(GLContext *aOrigin, GLuint aName)
{
    mTrackedPrograms.AppendElement(NamedResource(aOrigin, aName));
}

void
GLContext::CreatedShader(GLContext *aOrigin, GLuint aName)
{
    mTrackedShaders.AppendElement(NamedResource(aOrigin, aName));
}

void
GLContext::CreatedBuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedBuffers.AppendElement(NamedResource(aOrigin, aNames[i]));
    }
}

void
GLContext::CreatedTextures(GLContext *aOrigin, GLsizei aCount, GLuint *aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedTextures.AppendElement(NamedResource(aOrigin, aNames[i]));
    }
}

void
GLContext::CreatedFramebuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedFramebuffers.AppendElement(NamedResource(aOrigin, aNames[i]));
    }
}

void
GLContext::CreatedRenderbuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedRenderbuffers.AppendElement(NamedResource(aOrigin, aNames[i]));
    }
}

static void
RemoveNamesFromArray(GLContext *aOrigin, GLsizei aCount, GLuint *aNames, nsTArray<GLContext::NamedResource>& aArray)
{
    for (GLsizei j = 0; j < aCount; ++j) {
        GLuint name = aNames[j];
        // name 0 can be ignored
        if (name == 0)
            continue;

        for (uint32_t i = 0; i < aArray.Length(); ++i) {
            if (aArray[i].name == name) {
                aArray.RemoveElementAt(i);
                break;
            }
        }
    }
}

void
GLContext::DeletedProgram(GLContext *aOrigin, GLuint aName)
{
    RemoveNamesFromArray(aOrigin, 1, &aName, mTrackedPrograms);
}

void
GLContext::DeletedShader(GLContext *aOrigin, GLuint aName)
{
    RemoveNamesFromArray(aOrigin, 1, &aName, mTrackedShaders);
}

void
GLContext::DeletedBuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedBuffers);
}

void
GLContext::DeletedTextures(GLContext *aOrigin, GLsizei aCount, GLuint *aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedTextures);
}

void
GLContext::DeletedFramebuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedFramebuffers);
}

void
GLContext::DeletedRenderbuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedRenderbuffers);
}

static void
MarkContextDestroyedInArray(GLContext *aContext, nsTArray<GLContext::NamedResource>& aArray)
{
    for (uint32_t i = 0; i < aArray.Length(); ++i) {
        if (aArray[i].origin == aContext)
            aArray[i].originDeleted = true;
    }
}

void
GLContext::SharedContextDestroyed(GLContext *aChild)
{
    MarkContextDestroyedInArray(aChild, mTrackedPrograms);
    MarkContextDestroyedInArray(aChild, mTrackedShaders);
    MarkContextDestroyedInArray(aChild, mTrackedTextures);
    MarkContextDestroyedInArray(aChild, mTrackedFramebuffers);
    MarkContextDestroyedInArray(aChild, mTrackedRenderbuffers);
    MarkContextDestroyedInArray(aChild, mTrackedBuffers);
}

static void
ReportArrayContents(const char *title, const nsTArray<GLContext::NamedResource>& aArray)
{
    if (aArray.Length() == 0)
        return;

    printf_stderr("%s:\n", title);

    nsTArray<GLContext::NamedResource> copy(aArray);
    copy.Sort();

    GLContext *lastContext = NULL;
    for (uint32_t i = 0; i < copy.Length(); ++i) {
        if (lastContext != copy[i].origin) {
            if (lastContext)
                printf_stderr("\n");
            printf_stderr("  [%p - %s] ", copy[i].origin, copy[i].originDeleted ? "deleted" : "live");
            lastContext = copy[i].origin;
        }
        printf_stderr("%d ", copy[i].name);
    }
    printf_stderr("\n");
}

void
GLContext::ReportOutstandingNames()
{
    if (!DebugMode())
        return;

    printf_stderr("== GLContext %p Outstanding ==\n", this);

    ReportArrayContents("Outstanding Textures", mTrackedTextures);
    ReportArrayContents("Outstanding Buffers", mTrackedBuffers);
    ReportArrayContents("Outstanding Programs", mTrackedPrograms);
    ReportArrayContents("Outstanding Shaders", mTrackedShaders);
    ReportArrayContents("Outstanding Framebuffers", mTrackedFramebuffers);
    ReportArrayContents("Outstanding Renderbuffers", mTrackedRenderbuffers);
}

#endif /* DEBUG */

} /* namespace gl */
} /* namespace mozilla */
