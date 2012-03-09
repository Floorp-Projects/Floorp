/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Mark Steele <mwsteele@gmail.com>
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include <string.h>
#include <stdio.h>

#include "prlink.h"
#include "prenv.h"

#include "nsThreadUtils.h"

#include "gfxPlatform.h"
#include "GLContext.h"
#include "GLContextProvider.h"

#include "gfxCrashReporterUtils.h"
#include "gfxUtils.h"

#include "mozilla/Util.h" // for DebugOnly

namespace mozilla {
namespace gl {

#ifdef DEBUG
PRUintn GLContext::sCurrentGLContextTLS = -1;
#endif

PRUint32 GLContext::sDebugMode = 0;

// define this here since it's global to GLContextProvider, not any
// specific implementation
const ContextFormat ContextFormat::BasicRGBA32Format(ContextFormat::BasicRGBA32);

#define MAX_SYMBOL_LENGTH 128
#define MAX_SYMBOL_NAMES 5

// should match the order of GLExtensions
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
    "GL_EXT_framebuffer_blit",
    "GL_ANGLE_framebuffer_blit",
    "GL_EXT_framebuffer_multisample",
    "GL_ANGLE_framebuffer_multisample",
    "GL_OES_rgb8_rgba8",
    "GL_ARB_robustness",
    "GL_EXT_robustness",
    NULL
};

bool
LibrarySymbolLoader::OpenLibrary(const char *library)
{
    PRLibSpec lspec;
    lspec.type = PR_LibSpec_Pathname;
    lspec.value.pathname = library;

    mLibrary = PR_LoadLibraryWithFlags(lspec, PR_LD_LAZY | PR_LD_LOCAL);
    if (!mLibrary)
        return false;

    return true;
}

bool
LibrarySymbolLoader::LoadSymbols(SymLoadStruct *firstStruct, bool tryplatform, const char *prefix)
{
    return LoadSymbols(mLibrary, firstStruct, tryplatform ? mLookupFunc : nsnull, prefix);
}

PRFuncPtr
LibrarySymbolLoader::LookupSymbol(PRLibrary *lib,
                                  const char *sym,
                                  PlatformLookupFunction lookupFunction)
{
    PRFuncPtr res = 0;

    // try finding it in the library directly, if we have one
    if (lib) {
        res = PR_FindFunctionSymbol(lib, sym);
    }

    // then try looking it up via the lookup symbol
    if (!res && lookupFunction) {
        res = lookupFunction(sym);
    }

    // finally just try finding it in the process
    if (!res) {
        PRLibrary *leakedLibRef;
        res = PR_FindFunctionSymbolAndLibrary(sym, &leakedLibRef);
    }

    return res;
}

bool
LibrarySymbolLoader::LoadSymbols(PRLibrary *lib,
                                 SymLoadStruct *firstStruct,
                                 PlatformLookupFunction lookupFunction,
                                 const char *prefix)
{
    char sbuf[MAX_SYMBOL_LENGTH * 2];
    int failCount = 0;

    SymLoadStruct *ss = firstStruct;
    while (ss->symPointer) {
        *ss->symPointer = 0;

        for (int i = 0; i < MAX_SYMBOL_NAMES; i++) {
            if (ss->symNames[i] == nsnull)
                break;

            const char *s = ss->symNames[i];
            if (prefix && *prefix != 0) {
                strcpy(sbuf, prefix);
                strcat(sbuf, ss->symNames[i]);
                s = sbuf;
            }

            PRFuncPtr p = LookupSymbol(lib, s, lookupFunction);
            if (p) {
                *ss->symPointer = p;
                break;
            }
        }

        if (*ss->symPointer == 0) {
            fprintf (stderr, "Can't find symbol '%s'\n", ss->symNames[0]);
            failCount++;
        }

        ss++;
    }

    return failCount == 0 ? true : false;
}

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
                NS_RUNTIMEABORT("OpenGL ES 2.0 supported, but symbols could not be loaded.");
                mInitialized = false;
            }
        } else {
            SymLoadStruct symbols_desktop[] = {
                { (PRFuncPtr*) &mSymbols.fClearDepth, { "ClearDepth", NULL } },
                { (PRFuncPtr*) &mSymbols.fDepthRange, { "DepthRange", NULL } },
                { (PRFuncPtr*) &mSymbols.fReadBuffer, { "ReadBuffer", NULL } },
                { (PRFuncPtr*) &mSymbols.fMapBuffer, { "MapBuffer", NULL } },
                { (PRFuncPtr*) &mSymbols.fUnmapBuffer, { "UnmapBuffer", NULL } },
                { NULL, { NULL } },
            };

            if (!LoadSymbols(&symbols_desktop[0], trygl, prefix)) {
                NS_RUNTIMEABORT("Desktop symbols failed to load.");
                mInitialized = false;
            }
        }
    }

    const char *glVendorString;
    const char *glRendererString;

    if (mInitialized) {
        glVendorString = (const char *)fGetString(LOCAL_GL_VENDOR);
        const char *vendorMatchStrings[VendorOther] = {
                "Intel",
                "NVIDIA",
                "ATI",
                "Qualcomm"
        };
        mVendor = VendorOther;
        for (int i = 0; i < VendorOther; ++i) {
            if (DoesStringMatch(glVendorString, vendorMatchStrings[i])) {
                mVendor = i;
                break;
            }
        }

        glRendererString = (const char *)fGetString(LOCAL_GL_RENDERER);
        const char *rendererMatchStrings[RendererOther] = {
                "Adreno 200"
        };
        mRenderer = RendererOther;
        for (int i = 0; i < RendererOther; ++i) {
            if (DoesStringMatch(glRendererString, rendererMatchStrings[i])) {
                mRenderer = i;
                break;
            }
        }
    }

    if (mInitialized) {
#ifdef DEBUG
        static bool once = false;
        if (!once) {
            const char *vendors[VendorOther] = {
                "Intel",
                "NVIDIA",
                "ATI",
                "Qualcomm"
            };

            once = true;
            if (mVendor < VendorOther) {
                printf_stderr("OpenGL vendor ('%s') recognized as: %s\n",
                              glVendorString, vendors[mVendor]);
            } else {
                printf_stderr("OpenGL vendor ('%s') unrecognized\n", glVendorString);
            }
        }
#endif

        InitExtensions();

        NS_ASSERTION(!IsExtensionSupported(GLContext::ARB_pixel_buffer_object) ||
                     (mSymbols.fMapBuffer && mSymbols.fUnmapBuffer),
                     "ARB_pixel_buffer_object supported without glMapBuffer/UnmapBuffer being available!");

        if (SupportsRobustness()) {
            if (IsExtensionSupported(ARB_robustness)) {
                SymLoadStruct robustnessSymbols[] = {
                    { (PRFuncPtr*) &mSymbols.fGetGraphicsResetStatus, { "GetGraphicsResetStatusARB", NULL } },
                    { NULL, { NULL } },
                };

                if (!LoadSymbols(&robustnessSymbols[0], trygl, prefix)) {
                    NS_RUNTIMEABORT("GL supports ARB_robustness without supplying GetGraphicsResetStatusARB.");
                    mInitialized = false;
                } else {
                    mHasRobustness = true;
                }
            } else if (IsExtensionSupported(EXT_robustness)) {
                SymLoadStruct robustnessSymbols[] = {
                    { (PRFuncPtr*) &mSymbols.fGetGraphicsResetStatus, { "GetGraphicsResetStatusEXT", NULL } },
                    { NULL, { NULL } },
                };

                if (!LoadSymbols(&robustnessSymbols[0], trygl, prefix)) {
                    NS_RUNTIMEABORT("GL supports EGL_robustness without supplying GetGraphicsResetStatusEXT.");
                    mInitialized = false;
                } else {
                    mHasRobustness = true;
                }
            }
        }

        // Check for aux symbols based on extensions
        if (IsExtensionSupported(GLContext::ANGLE_framebuffer_blit) ||
            IsExtensionSupported(GLContext::EXT_framebuffer_blit)) {
            SymLoadStruct auxSymbols[] = {
                    { (PRFuncPtr*) &mSymbols.fBlitFramebuffer, { "BlitFramebuffer", "BlitFramebufferEXT", "BlitFramebufferANGLE", NULL } },
                    { NULL, { NULL } },
            };
            if (!LoadSymbols(&auxSymbols[0], trygl, prefix)) {
                NS_RUNTIMEABORT("GL supports framebuffer_blit without supplying glBlitFramebuffer");
                mInitialized = false;
            }
        }

        if (IsExtensionSupported(GLContext::ANGLE_framebuffer_multisample) ||
            IsExtensionSupported(GLContext::EXT_framebuffer_multisample)) {
            SymLoadStruct auxSymbols[] = {
                    { (PRFuncPtr*) &mSymbols.fRenderbufferStorageMultisample, { "RenderbufferStorageMultisample", "RenderbufferStorageMultisampleEXT", "RenderbufferStorageMultisampleANGLE", NULL } },
                    { NULL, { NULL } },
            };
            if (!LoadSymbols(&auxSymbols[0], trygl, prefix)) {
                NS_RUNTIMEABORT("GL supports framebuffer_multisample without supplying glRenderbufferStorageMultisample");
                mInitialized = false;
            }
        }
       
        // Load developer symbols, don't fail if we can't find them.
        SymLoadStruct auxSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetTexImage, { "GetTexImage", NULL } },
                { (PRFuncPtr*) &mSymbols.fGetTexLevelParameteriv, { "GetTexLevelParameteriv", NULL } },
                { NULL, { NULL } },
        };
        LoadSymbols(&auxSymbols[0], trygl, prefix);
    }

    if (mInitialized) {
        GLint v[4];

        fGetIntegerv(LOCAL_GL_SCISSOR_BOX, v);
        mScissorStack.AppendElement(nsIntRect(v[0], v[1], v[2], v[3]));

        fGetIntegerv(LOCAL_GL_VIEWPORT, v);
        mViewportStack.AppendElement(nsIntRect(v[0], v[1], v[2], v[3]));

        fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
        fGetIntegerv(LOCAL_GL_MAX_RENDERBUFFER_SIZE, &mMaxRenderbufferSize);
        mMaxTextureImageSize = mMaxTextureSize;

        mSupport_ES_ReadPixels_BGRA_UByte = false;
        if (mIsGLES2) {
            if (IsExtensionSupported(gl::GLContext::EXT_bgra)) {
                mSupport_ES_ReadPixels_BGRA_UByte = true;
            } else if (IsExtensionSupported(gl::GLContext::EXT_read_format_bgra) ||
                       IsExtensionSupported(gl::GLContext::IMG_read_format)) {
                GLint auxFormat = 0;
                GLint auxType = 0;

                fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT, &auxFormat);
                fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_TYPE, &auxType);

                if (auxFormat == LOCAL_GL_BGRA && auxType == LOCAL_GL_UNSIGNED_BYTE)
                    mSupport_ES_ReadPixels_BGRA_UByte = true;
            }
        }

        UpdateActualFormat();
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

    if (mInitialized)
        reporter.SetSuccessful();
    else {
        // if initialization fails, ensure all symbols are zero, to avoid hard-to-understand bugs
        mSymbols.Zero();
    }

    return mInitialized;
}

void
GLContext::InitExtensions()
{
    MakeCurrent();
    const GLubyte *extensions = fGetString(LOCAL_GL_EXTENSIONS);
    if (!extensions)
        return;

    char *exts = strdup((char *)extensions);

#ifdef DEBUG
    static bool once = false;
#else
    const bool once = true;
#endif

    if (!once) {
        printf_stderr("GL extensions: %s\n", exts);
    }

    char *s = exts;
    bool done = false;
    while (!done) {
        char *space = strchr(s, ' ');
        if (space) {
            *space = '\0';
        } else {
            done = true;
        }

        for (int i = 0; sExtensionNames[i]; ++i) {
            if (strcmp(s, sExtensionNames[i]) == 0) {
                if (!once) {
                    printf_stderr("Found extension %s\n", s);
                }
                mAvailableExtensions[i] = 1;
            }
        }

        s = space+1;
    }

    free(exts);

#ifdef DEBUG
    once = true;
#endif
}

bool
GLContext::IsExtensionSupported(const char *extension)
{
    return ListHasExtension(fGetString(LOCAL_GL_EXTENSIONS), extension);
}

bool
GLContext::CanUploadSubTextures()
{
    // There are certain GPUs that we don't want to use glTexSubImage2D on
    // because that function can be very slow and/or buggy

    return !(Renderer() == RendererAdreno200);
}

// Common code for checking for both GL extensions and GLX extensions.
bool
GLContext::ListHasExtension(const GLubyte *extensions, const char *extension)
{
    // fix bug 612572 - we were crashing as we were calling this function with extensions==null
    if (extensions == nsnull || extension == nsnull)
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
                              bool aUseNearestFilter)
{
    MakeCurrent();

    GLuint texture;
    fGenTextures(1, &texture);

    fActiveTexture(LOCAL_GL_TEXTURE0);
    fBindTexture(LOCAL_GL_TEXTURE_2D, texture);

    GLint texfilter = aUseNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, aWrapMode);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, aWrapMode);

    return CreateBasicTextureImage(texture, aSize, aWrapMode, aContentType, this);
}

void GLContext::ApplyFilterToBoundTexture(gfxPattern::GraphicsFilter aFilter)
{
    if (aFilter == gfxPattern::FILTER_NEAREST) {
        fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_NEAREST);
    } else {
        if (aFilter != gfxPattern::FILTER_GOOD) {
            NS_WARNING("Unsupported filter type!");
        }
        fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
       fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    }
}

BasicTextureImage::~BasicTextureImage()
{
    GLContext *ctx = mGLContext;
    if (ctx->IsDestroyed() || !NS_IsMainThread()) {
        ctx = ctx->GetSharedContext();
    }

    // If we have a context, then we need to delete the texture;
    // if we don't have a context (either real or shared),
    // then they went away when the contex was deleted, because it
    // was the only one that had access to it.
    if (ctx && !ctx->IsDestroyed()) {
        mGLContext->MakeCurrent();
        mGLContext->fDeleteTextures(1, &mTexture);
    }
}

gfxASurface*
BasicTextureImage::BeginUpdate(nsIntRegion& aRegion)
{
    NS_ASSERTION(!mUpdateSurface, "BeginUpdate() without EndUpdate()?");

    // determine the region the client will need to repaint
    if (mGLContext->CanUploadSubTextures()) {
        GetUpdateRegion(aRegion);
    } else {
        aRegion = nsIntRect(nsIntPoint(0, 0), mSize);
    }

    mUpdateRegion = aRegion;

    nsIntRect rgnSize = mUpdateRegion.GetBounds();
    if (!nsIntRect(nsIntPoint(0, 0), mSize).Contains(rgnSize)) {
        NS_ERROR("update outside of image");
        return NULL;
    }

    ImageFormat format =
        (GetContentType() == gfxASurface::CONTENT_COLOR) ?
        gfxASurface::ImageFormatRGB24 : gfxASurface::ImageFormatARGB32;
    mUpdateSurface =
        GetSurfaceForUpdate(gfxIntSize(rgnSize.width, rgnSize.height), format);

    if (!mUpdateSurface || mUpdateSurface->CairoStatus()) {
        mUpdateSurface = NULL;
        return NULL;
    }

    mUpdateSurface->SetDeviceOffset(gfxPoint(-rgnSize.x, -rgnSize.y));

    return mUpdateSurface;
}

void
BasicTextureImage::GetUpdateRegion(nsIntRegion& aForRegion)
{
  // if the texture hasn't been initialized yet, or something important
  // changed, we need to recreate our backing surface and force the
  // client to paint everything
  if (mTextureState != Valid)
      aForRegion = nsIntRect(nsIntPoint(0, 0), mSize);
}

void
BasicTextureImage::EndUpdate()
{
    NS_ASSERTION(!!mUpdateSurface, "EndUpdate() without BeginUpdate()?");

    // FIXME: this is the slow boat.  Make me fast (with GLXPixmap?).

    // Undo the device offset that BeginUpdate set; doesn't much matter for us here,
    // but important if we ever do anything directly with the surface.
    mUpdateSurface->SetDeviceOffset(gfxPoint(0, 0));

    bool relative = FinishedSurfaceUpdate();

    mShaderType =
        mGLContext->UploadSurfaceToTexture(mUpdateSurface,
                                           mUpdateRegion,
                                           mTexture,
                                           mTextureState == Created,
                                           mUpdateOffset,
                                           relative);
    FinishedSurfaceUpload();

    mUpdateSurface = nsnull;
    mTextureState = Valid;
}

void
BasicTextureImage::BindTexture(GLenum aTextureUnit)
{
    mGLContext->fActiveTexture(aTextureUnit);
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
}

void
BasicTextureImage::ApplyFilter()
{
  mGLContext->ApplyFilterToBoundTexture(mFilter);
}


already_AddRefed<gfxASurface>
BasicTextureImage::GetSurfaceForUpdate(const gfxIntSize& aSize, ImageFormat aFmt)
{
    return gfxPlatform::GetPlatform()->
        CreateOffscreenSurface(aSize, gfxASurface::ContentFromFormat(aFmt));
}

bool
BasicTextureImage::FinishedSurfaceUpdate()
{
    return false;
}

void
BasicTextureImage::FinishedSurfaceUpload()
{
}

bool 
BasicTextureImage::DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom /* = nsIntPoint(0, 0) */)
{
    nsIntRect bounds = aRegion.GetBounds();
    nsIntRegion region;
    if (mTextureState != Valid) {
        bounds = nsIntRect(0, 0, mSize.width, mSize.height);
        region = nsIntRegion(bounds);
    } else {
        region = aRegion;
    }

    mShaderType =
        mGLContext->UploadSurfaceToTexture(aSurf,
                                           region,
                                           mTexture,
                                           mTextureState == Created,
                                           bounds.TopLeft() + aFrom,
                                           false);
    mTextureState = Valid;
    return true;
}

void
BasicTextureImage::Resize(const nsIntSize& aSize)
{
    NS_ASSERTION(!mUpdateSurface, "Resize() while in update?");

    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

    mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                            0,
                            LOCAL_GL_RGBA,
                            aSize.width,
                            aSize.height,
                            0,
                            LOCAL_GL_RGBA,
                            LOCAL_GL_UNSIGNED_BYTE,
                            NULL);

    mTextureState = Allocated;
    mSize = aSize;
}

TiledTextureImage::TiledTextureImage(GLContext* aGL,
                                     nsIntSize aSize,
                                     TextureImage::ContentType aContentType,
                                     bool aUseNearestFilter)
    : TextureImage(aSize, LOCAL_GL_CLAMP_TO_EDGE, aContentType, aUseNearestFilter)
    , mCurrentImage(0)
    , mInUpdate(false)
    , mGL(aGL)
    , mUseNearestFilter(aUseNearestFilter)
    , mTextureState(Created)
{
    mTileSize = mGL->GetMaxTextureSize();
    if (aSize != nsIntSize(0,0)) {
        Resize(aSize);
    }
}

TiledTextureImage::~TiledTextureImage()
{
}

bool 
TiledTextureImage::DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom /* = nsIntPoint(0, 0) */)
{
    nsIntRegion region;

    if (mTextureState != Valid) {
        nsIntRect bounds = nsIntRect(0, 0, mSize.width, mSize.height);
        region = nsIntRegion(bounds);
    } else {
        region = aRegion;
    }

    bool result = true;
    for (unsigned i = 0; i < mImages.Length(); i++) {
        int xPos = (i % mColumns) * mTileSize;
        int yPos = (i / mColumns) * mTileSize;
        nsIntRegion tileRegion;
        nsIntRect tileRect = nsIntRect(nsIntPoint(xPos, yPos), mImages[i]->GetSize());
        tileRegion.And(region, tileRect); // intersect with tile
        if (tileRegion.IsEmpty())
            continue;
        if (mGL->CanUploadSubTextures()) {
          tileRegion.MoveBy(-xPos, -yPos); // translate into tile local space
        } else {
          // If sub-textures are unsupported, expand to tile boundaries
          tileRect.x = tileRect.y = 0;
          tileRegion = nsIntRegion(tileRect);
        }
        result &= mImages[i]->DirectUpdate(aSurf,
                                           tileRegion,
                                           aFrom + nsIntPoint(xPos, yPos));
    }
    mShaderType = mImages[0]->GetShaderProgramType();
    mTextureState = Valid;
    return result;
}

void
TiledTextureImage::GetUpdateRegion(nsIntRegion& aForRegion)
{
    if (mTextureState != Valid) {
        // if the texture hasn't been initialized yet, or something important
        // changed, we need to recreate our backing surface and force the
        // client to paint everything
        aForRegion = nsIntRect(nsIntPoint(0, 0), mSize);
        return;
    }

    nsIntRegion newRegion;

    // We need to query each texture with the region it will be drawing and
    // set aForRegion to be the combination of all of these regions
    for (unsigned i = 0; i < mImages.Length(); i++) {
        int xPos = (i % mColumns) * mTileSize;
        int yPos = (i / mColumns) * mTileSize;
        nsIntRect imageRect = nsIntRect(nsIntRect(nsIntPoint(xPos,yPos), mImages[i]->GetSize()));

        if (aForRegion.Intersects(imageRect)) {
            // Make a copy of the region
            nsIntRegion subRegion;
            subRegion.And(aForRegion, imageRect);
            // Translate it into tile-space
            subRegion.MoveBy(-xPos, -yPos);
            // Query region
            mImages[i]->GetUpdateRegion(subRegion);
            // Translate back
            subRegion.MoveBy(xPos, yPos);
            // Add to the accumulated region
            newRegion.Or(newRegion, subRegion);
        }
    }

    aForRegion = newRegion;
}

gfxASurface*
TiledTextureImage::BeginUpdate(nsIntRegion& aRegion)
{
    NS_ASSERTION(!mInUpdate, "nested update");
    mInUpdate = true;

    // Note, we don't call GetUpdateRegion here as if the updated region is
    // fully contained in a single tile, we get to avoid iterating through
    // the tiles again (and a little copying).
    if (mTextureState != Valid)
    {
        // if the texture hasn't been initialized yet, or something important
        // changed, we need to recreate our backing surface and force the
        // client to paint everything
        aRegion = nsIntRect(nsIntPoint(0, 0), mSize);
    }

    nsIntRect bounds = aRegion.GetBounds();

    for (unsigned i = 0; i < mImages.Length(); i++) {
        int xPos = (i % mColumns) * mTileSize;
        int yPos = (i / mColumns) * mTileSize;
        nsIntRegion imageRegion = nsIntRegion(nsIntRect(nsIntPoint(xPos,yPos), mImages[i]->GetSize()));

        // a single Image can handle this update request
        if (imageRegion.Contains(aRegion)) {
            // adjust for tile offset
            aRegion.MoveBy(-xPos, -yPos);
            // forward the actual call
            nsRefPtr<gfxASurface> surface = mImages[i]->BeginUpdate(aRegion);
            // caller expects container space
            aRegion.MoveBy(xPos, yPos);
            // Correct the device offset
            gfxPoint offset = surface->GetDeviceOffset();
            surface->SetDeviceOffset(gfxPoint(offset.x - xPos,
                                              offset.y - yPos));
            // we don't have a temp surface
            mUpdateSurface = nsnull;
            // remember which image to EndUpdate
            mCurrentImage = i;
            return surface.get();
        }
    }

    // Get the real updated region, taking into account the capabilities of
    // each TextureImage tile
    GetUpdateRegion(aRegion);
    mUpdateRegion = aRegion;
    bounds = aRegion.GetBounds();

    // update covers multiple Images - create a temp surface to paint in
    gfxASurface::gfxImageFormat format =
        (GetContentType() == gfxASurface::CONTENT_COLOR) ?
        gfxASurface::ImageFormatRGB24 : gfxASurface::ImageFormatARGB32;
    mUpdateSurface = gfxPlatform::GetPlatform()->
        CreateOffscreenSurface(gfxIntSize(bounds.width, bounds.height), gfxASurface::ContentFromFormat(format));
    mUpdateSurface->SetDeviceOffset(gfxPoint(-bounds.x, -bounds.y));

    return mUpdateSurface;
}

void
TiledTextureImage::EndUpdate()
{
    NS_ASSERTION(mInUpdate, "EndUpdate not in update");
    if (!mUpdateSurface) { // update was to a single TextureImage
        mImages[mCurrentImage]->EndUpdate();
        mInUpdate = false;
        mTextureState = Valid;
        mShaderType = mImages[mCurrentImage]->GetShaderProgramType();
        return;
    }

    // upload tiles from temp surface
    for (unsigned i = 0; i < mImages.Length(); i++) {
        int xPos = (i % mColumns) * mTileSize;
        int yPos = (i / mColumns) * mTileSize;
        nsIntRect imageRect = nsIntRect(nsIntPoint(xPos,yPos), mImages[i]->GetSize());

        nsIntRegion subregion;
        subregion.And(mUpdateRegion, imageRect);
        if (subregion.IsEmpty())
            continue;
        subregion.MoveBy(-xPos, -yPos); // Tile-local space
        // copy tile from temp surface
        gfxASurface* surf = mImages[i]->BeginUpdate(subregion);
        nsRefPtr<gfxContext> ctx = new gfxContext(surf);
        gfxUtils::ClipToRegion(ctx, subregion);
        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
        ctx->SetSource(mUpdateSurface, gfxPoint(-xPos, -yPos));
        ctx->Paint();
        mImages[i]->EndUpdate();
    }

    mUpdateSurface = nsnull;
    mInUpdate = false;
    mShaderType = mImages[0]->GetShaderProgramType();
    mTextureState = Valid;
}

void TiledTextureImage::BeginTileIteration()
{
    mCurrentImage = 0;
}

bool TiledTextureImage::NextTile()
{
    if (mCurrentImage + 1 < mImages.Length()) {
        mCurrentImage++;
        return true;
    }
    return false;
}

nsIntRect TiledTextureImage::GetTileRect()
{
    nsIntRect rect = mImages[mCurrentImage]->GetTileRect();
    unsigned int xPos = (mCurrentImage % mColumns) * mTileSize;
    unsigned int yPos = (mCurrentImage / mColumns) * mTileSize;
    rect.MoveBy(xPos, yPos);
    return rect;
}

void
TiledTextureImage::BindTexture(GLenum aTextureUnit)
{
    mImages[mCurrentImage]->BindTexture(aTextureUnit);
}

void
TiledTextureImage::ApplyFilter()
{
   mGL->ApplyFilterToBoundTexture(mFilter);
}

/*
 * simple resize, just discards everything. we can be more clever just
 * adding or discarding tiles, but do we want this?
 */
void TiledTextureImage::Resize(const nsIntSize& aSize)
{
    if (mSize == aSize && mTextureState != Created) {
        return;
    }
    mSize = aSize;
    mImages.Clear();
    // calculate rows and columns, rounding up
    mColumns = (aSize.width  + mTileSize - 1) / mTileSize;
    mRows    = (aSize.height + mTileSize - 1) / mTileSize;

    for (unsigned int row = 0; row < mRows; row++) {
      for (unsigned int col = 0; col < mColumns; col++) {
          nsIntSize size( // use tilesize first, then the remainder
                  (col+1) * mTileSize > (unsigned int)aSize.width  ? aSize.width  % mTileSize : mTileSize,
                  (row+1) * mTileSize > (unsigned int)aSize.height ? aSize.height % mTileSize : mTileSize);
          nsRefPtr<TextureImage> teximg =
                  mGL->TileGenFunc(size, mContentType, mUseNearestFilter);
          mImages.AppendElement(teximg.forget());
      }
    }
    mTextureState = Allocated;
}

PRUint32 TiledTextureImage::GetTileCount()
{
    return mImages.Length();
}

bool
GLContext::ResizeOffscreenFBO(const gfxIntSize& aSize, const bool aUseReadFBO, const bool aDisableAA)
{
    if (!IsOffscreenSizeAllowed(aSize))
        return false;

    MakeCurrent();

    const bool alpha = mCreationFormat.alpha > 0;
    const int depth = mCreationFormat.depth;
    const int stencil = mCreationFormat.stencil;
    int samples = mCreationFormat.samples;

    GLint maxSamples = 0;
    if (SupportsFramebufferMultisample() && !aDisableAA)
        fGetIntegerv(LOCAL_GL_MAX_SAMPLES, &maxSamples);

    samples = NS_MIN(samples, maxSamples);

    const bool useDrawMSFBO = (samples > 0);

    if (!useDrawMSFBO && !aUseReadFBO) {
        // Early out, as no FBO resize work is necessary.
        return true;
    }

    GLuint curBoundFramebufferDraw = 0;
    GLuint curBoundFramebufferRead = 0;
    GLuint curBoundRenderbuffer = 0;
    GLuint curBoundTexture = 0;

    GLint viewport[4];

    const bool useDepthStencil =
            !mIsGLES2 || IsExtensionSupported(OES_packed_depth_stencil);

    // save a few things for later restoring
    curBoundFramebufferDraw = GetBoundDrawFBO();
    curBoundFramebufferRead = GetBoundReadFBO();
    fGetIntegerv(LOCAL_GL_RENDERBUFFER_BINDING, (GLint*) &curBoundRenderbuffer);
    fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, (GLint*) &curBoundTexture);
    fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);

    // the context format of what we're defining
    // This becomes mActualFormat on success
    ContextFormat cf(mCreationFormat);

    // Create everything we need for the resize, so if it fails, we haven't broken anything
    // If successful, these new resized objects will replace their associated member vars in GLContext
    GLuint newOffscreenDrawFBO = 0;
    GLuint newOffscreenReadFBO = 0;
    GLuint newOffscreenTexture = 0;
    GLuint newOffscreenColorRB = 0;
    GLuint newOffscreenDepthRB = 0;
    GLuint newOffscreenStencilRB = 0;

    // Create the buffers and texture
    if (aUseReadFBO) {
        fGenFramebuffers(1, &newOffscreenReadFBO);
        fGenTextures(1, &newOffscreenTexture);
    }

    if (useDrawMSFBO) {
        fGenFramebuffers(1, &newOffscreenDrawFBO);
        fGenRenderbuffers(1, &newOffscreenColorRB);
    } else {
        newOffscreenDrawFBO = newOffscreenReadFBO;
    }

    if (depth && stencil && useDepthStencil) {
        fGenRenderbuffers(1, &newOffscreenDepthRB);
    } else {
        if (depth) {
            fGenRenderbuffers(1, &newOffscreenDepthRB);
        }

        if (stencil) {
            fGenRenderbuffers(1, &newOffscreenStencilRB);
        }
    }

   // Allocate texture
    if (aUseReadFBO) {
        fBindTexture(LOCAL_GL_TEXTURE_2D, newOffscreenTexture);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

        if (alpha) {
            fTexImage2D(LOCAL_GL_TEXTURE_2D,
                        0,
                        LOCAL_GL_RGBA,
                        aSize.width, aSize.height,
                        0,
                        LOCAL_GL_RGBA,
                        LOCAL_GL_UNSIGNED_BYTE,
                        NULL);

            cf.red = cf.green = cf.blue = cf.alpha = 8;
        } else {
            fTexImage2D(LOCAL_GL_TEXTURE_2D,
                        0,
                        LOCAL_GL_RGB,
                        aSize.width, aSize.height,
                        0,
                        LOCAL_GL_RGB,
#ifdef XP_WIN
                        LOCAL_GL_UNSIGNED_BYTE,
#else
                        mIsGLES2 ? LOCAL_GL_UNSIGNED_SHORT_5_6_5
                                 : LOCAL_GL_UNSIGNED_BYTE,
#endif
                        NULL);

#ifdef XP_WIN
            cf.red = cf.green = cf.blue = 8;
#else
            cf.red = 5;
            cf.green = 6;
            cf.blue = 5;
#endif
            cf.alpha = 0;
        }
    }
    cf.samples = samples;

    // Allocate color buffer
    if (useDrawMSFBO) {
        GLenum colorFormat;
        if (!mIsGLES2 || IsExtensionSupported(OES_rgb8_rgba8))
            colorFormat = alpha ? LOCAL_GL_RGBA8 : LOCAL_GL_RGB8;
        else
            colorFormat = alpha ? LOCAL_GL_RGBA4 : LOCAL_GL_RGB565;

        fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, newOffscreenColorRB);
        fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER,
                                        samples,
                                        colorFormat,
                                        aSize.width, aSize.height);
    }

    // Allocate depth and stencil buffers
    if (depth && stencil && useDepthStencil) {
        fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, newOffscreenDepthRB);
        if (useDrawMSFBO) {
            fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER,
                                            samples,
                                            LOCAL_GL_DEPTH24_STENCIL8,
                                            aSize.width, aSize.height);
        } else {
            fRenderbufferStorage(LOCAL_GL_RENDERBUFFER,
                                 LOCAL_GL_DEPTH24_STENCIL8,
                                 aSize.width, aSize.height);
        }
        cf.depth = 24;
        cf.stencil = 8;
    } else {
        if (depth) {
            GLenum depthType;
            if (mIsGLES2) {
                if (IsExtensionSupported(OES_depth32)) {
                    depthType = LOCAL_GL_DEPTH_COMPONENT32;
                    cf.depth = 32;
                } else if (IsExtensionSupported(OES_depth24)) {
                    depthType = LOCAL_GL_DEPTH_COMPONENT24;
                    cf.depth = 24;
                } else {
                    depthType = LOCAL_GL_DEPTH_COMPONENT16;
                   cf.depth = 16;
                }
            } else {
                depthType = LOCAL_GL_DEPTH_COMPONENT24;
                cf.depth = 24;
            }

            fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, newOffscreenDepthRB);
            if (useDrawMSFBO) {
                fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER,
                                                samples,
                                                depthType,
                                                aSize.width, aSize.height);
            } else {
                fRenderbufferStorage(LOCAL_GL_RENDERBUFFER,
                                     depthType,
                                     aSize.width, aSize.height);
            }
        }

        if (stencil) {
            fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, newOffscreenStencilRB);
            if (useDrawMSFBO) {
                fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER,
                                                samples,
                                                LOCAL_GL_STENCIL_INDEX8,
                                                aSize.width, aSize.height);
            } else {
                fRenderbufferStorage(LOCAL_GL_RENDERBUFFER,
                                     LOCAL_GL_STENCIL_INDEX8,
                                     aSize.width, aSize.height);
            }
            cf.stencil = 8;
        }
    }

    // Now assemble the FBO
    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, newOffscreenDrawFBO);    // If we're not using a separate draw FBO, this will be the read FBO
    if (useDrawMSFBO) {
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_COLOR_ATTACHMENT0,
                                 LOCAL_GL_RENDERBUFFER,
                                 newOffscreenColorRB);
    }

    if (depth && stencil && useDepthStencil) {
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER,
                                 newOffscreenDepthRB);
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_STENCIL_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER,
                                 newOffscreenDepthRB);
    } else {
        if (depth) {
            fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                     LOCAL_GL_DEPTH_ATTACHMENT,
                                     LOCAL_GL_RENDERBUFFER,
                                     newOffscreenDepthRB);
        }

        if (stencil) {
            fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                     LOCAL_GL_STENCIL_ATTACHMENT,
                                     LOCAL_GL_RENDERBUFFER,
                                     newOffscreenStencilRB);
        }
    }

    if (aUseReadFBO) {
        fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, newOffscreenReadFBO);
        fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                              LOCAL_GL_COLOR_ATTACHMENT0,
                              LOCAL_GL_TEXTURE_2D,
                              newOffscreenTexture,
                              0);
    }

    // We should be all resized.  Check for framebuffer completeness.
    GLenum status;
    bool framebuffersComplete = true;

    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, newOffscreenDrawFBO);
    status = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        NS_WARNING("DrawFBO: Incomplete");
#ifdef DEBUG
        printf_stderr("Framebuffer status: %X\n", status);
#endif
        framebuffersComplete = false;
    }

    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, newOffscreenReadFBO);
    status = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        NS_WARNING("ReadFBO: Incomplete");
#ifdef DEBUG
        printf_stderr("Framebuffer status: %X\n", status);
#endif
        framebuffersComplete = false;
    }

    if (!framebuffersComplete) {
        NS_WARNING("Error resizing offscreen framebuffer -- framebuffer(s) not complete");

        // Clean up the mess
        fDeleteFramebuffers(1, &newOffscreenDrawFBO);
        fDeleteFramebuffers(1, &newOffscreenReadFBO);
        fDeleteTextures(1, &newOffscreenTexture);
        fDeleteRenderbuffers(1, &newOffscreenColorRB);
        fDeleteRenderbuffers(1, &newOffscreenDepthRB);
        fDeleteRenderbuffers(1, &newOffscreenStencilRB);

        BindReadFBO(curBoundFramebufferRead);
        BindDrawFBO(curBoundFramebufferDraw);
        fBindTexture(LOCAL_GL_TEXTURE_2D, curBoundTexture);
        fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, curBoundRenderbuffer);
        fViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

        return false;
    }

    // Success, so delete the old and busted
    fDeleteFramebuffers(1, &mOffscreenDrawFBO);
    fDeleteFramebuffers(1, &mOffscreenReadFBO);
    fDeleteTextures(1, &mOffscreenTexture);
    fDeleteRenderbuffers(1, &mOffscreenColorRB);
    fDeleteRenderbuffers(1, &mOffscreenDepthRB);
    fDeleteRenderbuffers(1, &mOffscreenStencilRB);

    // Update currently bound references if we're changing what they were point to
    // This way we don't rebind to old buffers when we're done here
    if (curBoundFramebufferDraw == mOffscreenDrawFBO)
        curBoundFramebufferDraw = newOffscreenDrawFBO;
    if (curBoundFramebufferRead == mOffscreenReadFBO)
        curBoundFramebufferRead = newOffscreenReadFBO;
    if (curBoundTexture == mOffscreenTexture)
        curBoundTexture = newOffscreenTexture;
    if (curBoundRenderbuffer == mOffscreenColorRB)
        curBoundRenderbuffer = newOffscreenColorRB;
    else if (curBoundRenderbuffer == mOffscreenDepthRB)
        curBoundRenderbuffer = newOffscreenDepthRB;
    else if (curBoundRenderbuffer == mOffscreenStencilRB)
        curBoundRenderbuffer = newOffscreenStencilRB;

    // Replace with the new hotness
    mOffscreenDrawFBO = newOffscreenDrawFBO;
    mOffscreenReadFBO = newOffscreenReadFBO;
    mOffscreenTexture = newOffscreenTexture;
    mOffscreenColorRB = newOffscreenColorRB;
    mOffscreenDepthRB = newOffscreenDepthRB;
    mOffscreenStencilRB = newOffscreenStencilRB;

    mOffscreenSize = aSize;
    mOffscreenActualSize = aSize;

    mActualFormat = cf;

#ifdef DEBUG
    if (DebugMode()) {
        printf_stderr("Resized %dx%d offscreen FBO: r: %d g: %d b: %d a: %d depth: %d stencil: %d samples: %d\n",
                      mOffscreenActualSize.width, mOffscreenActualSize.height,
                      mActualFormat.red, mActualFormat.green, mActualFormat.blue, mActualFormat.alpha,
                      mActualFormat.depth, mActualFormat.stencil, mActualFormat.samples);
    }
#endif

    // Make sure we know that the buffers are new and thus dirty:
    ForceDirtyFBOs();

    // We're good, and the framebuffer is already attached.
    // Now restore the GL state back to what it was before the resize took place.
    // If the user was using fb 0, this will bind the offscreen framebuffer we
    // just created.
    BindDrawFBO(curBoundFramebufferDraw);
    BindReadFBO(curBoundFramebufferRead);
    fBindTexture(LOCAL_GL_TEXTURE_2D, curBoundTexture);
    fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, curBoundRenderbuffer);
    fViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    return true;
}

void
GLContext::DeleteOffscreenFBO()
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

    MakeCurrent();
    DeleteOffscreenFBO();

    fDeleteProgram(mBlitProgram);
    mBlitProgram = 0;
    fDeleteFramebuffers(1, &mBlitFramebuffer);
    mBlitFramebuffer = 0;

    mSymbols.Zero();
}

static void SwapRAndBComponents(gfxImageSurface* aSurf)
{
  gfxIntSize size = aSurf->GetSize();
  for (int j = 0; j < size.height; ++j) {
    PRUint32 *row = (PRUint32*) (aSurf->Data() + aSurf->Stride() * j);
    for (int i = 0; i < size.width; ++i) {
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
    fFinish();
    fActiveTexture(LOCAL_GL_TEXTURE0);
    fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);

    gfxIntSize size;
    fGetTexLevelParameteriv(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_TEXTURE_WIDTH, &size.width);
    fGetTexLevelParameteriv(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_TEXTURE_HEIGHT, &size.height);
    
    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(size, gfxASurface::ImageFormatARGB32);
    if (!surf || surf->CairoStatus()) {
        return NULL;
    }

    PRUint32 currentPackAlignment = 0;
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
        isurf = nsnull;
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

void
GLContext::ReadPixelsIntoImageSurface(GLint aX, GLint aY,
                                      GLsizei aWidth, GLsizei aHeight,
                                      gfxImageSurface *aDest)
{
    MakeCurrent();

    if (aDest->Format() != gfxASurface::ImageFormatARGB32 &&
        aDest->Format() != gfxASurface::ImageFormatRGB24)
    {
        NS_WARNING("ReadPixelsIntoImageSurface called with invalid image format");
        return;
    }

    if (aDest->Width() != aWidth ||
        aDest->Height() != aHeight ||
        aDest->Stride() != aWidth * 4)
    {
        NS_WARNING("ReadPixelsIntoImageSurface called with wrong size or stride surface");
        return;
    }

    GLint currentPackAlignment = 0;
    fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, &currentPackAlignment);
    fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 4);

    // defaults for desktop
    GLenum format = LOCAL_GL_BGRA;
    GLenum datatype = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
    bool swap = false;

    if (IsGLES2()) {
        datatype = LOCAL_GL_UNSIGNED_BYTE;

        if (mSupport_ES_ReadPixels_BGRA_UByte) {
            format = LOCAL_GL_BGRA;
        } else {
            format = LOCAL_GL_RGBA;
            swap = true;
        }
    }

    fReadPixels(0, 0, aWidth, aHeight,
                format, datatype,
                aDest->Data());

    if (swap) {
        // swap B and R bytes
        for (int j = 0; j < aHeight; ++j) {
            PRUint32 *row = (PRUint32*) (aDest->Data() + aDest->Stride() * j);
            for (int i = 0; i < aWidth; ++i) {
                *row = (*row & 0xff00ff00) | ((*row & 0xff) << 16) | ((*row & 0xff0000) >> 16);
                row++;
            }
        }
    }

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
    if (mVendor == VendorQualcomm) {
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
            if (aSrc->GetWrapMode() == LOCAL_GL_REPEAT) {
                rects.addRect(/* dest rectangle */
                        dx0, dy0, dx1, dy1,
                        /* tex coords */
                        srcSubRect.x / float(srcSize.width),
                        srcSubRect.y / float(srcSize.height),
                        srcSubRect.XMost() / float(srcSize.width),
                        srcSubRect.YMost() / float(srcSize.height));
            } else {
                DecomposeIntoNoRepeatTriangles(srcSubRect, srcSize, rects);

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
    if (mVendor == VendorQualcomm) {
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
                                  bool aPixelBuffer)
{
    bool textureInited = aOverwrite ? false : true;
    MakeCurrent();
    fActiveTexture(LOCAL_GL_TEXTURE0);
  
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
    GLenum internalformat;
    GLenum type;
    PRInt32 pixelSize = gfxASurface::BytePerPixelFromFormat(imageSurface->Format());
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

    PRInt32 stride = imageSurface->Stride();

#ifndef USE_GLES2
    internalformat = LOCAL_GL_RGBA;
#else
    internalformat = format;
#endif

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
                       internalformat,
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
#ifdef USE_GLES2

    NS_ASSERTION(format == internalformat,
                 "format and internalformat not the same for glTexImage2D on GLES2");

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
#else
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
#endif
}

void
GLContext::TexSubImage2D(GLenum target, GLint level,
                         GLint xoffset, GLint yoffset,
                         GLsizei width, GLsizei height, GLsizei stride,
                         GLint pixelsize, GLenum format,
                         GLenum type, const GLvoid* pixels)
{
#ifdef USE_GLES2
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
#else
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
#endif
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
                                  GLfloat tx0, GLfloat ty0, GLfloat tx1, GLfloat ty1)
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
                                          RectTriangles& aRects)
{
    // normalize this
    nsIntRect tcr(aTexCoordRect);
    while (tcr.x > aTexSize.width)
        tcr.x -= aTexSize.width;
    while (tcr.y > aTexSize.height)
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
        aRects.addRect(0.0f, 0.0f, 1.0f, 1.0f,
                       tl[0], tl[1], br[0], br[1]);
    } else if (!xwrap && ywrap) {
        GLfloat ymid = (1.0f - tl[1]) / ylen;
        aRects.addRect(0.0f, 0.0f,
                       1.0f, ymid,
                       tl[0], tl[1],
                       br[0], 1.0f);
        aRects.addRect(0.0f, ymid,
                       1.0f, 1.0f,
                       tl[0], 0.0f,
                       br[0], br[1]);
    } else if (xwrap && !ywrap) {
        GLfloat xmid = (1.0f - tl[0]) / xlen;
        aRects.addRect(0.0f, 0.0f,
                       xmid, 1.0f,
                       tl[0], tl[1],
                       1.0f, br[1]);
        aRects.addRect(xmid, 0.0f,
                       1.0f, 1.0f,
                       0.0f, tl[1],
                       br[0], br[1]);
    } else {
        GLfloat xmid = (1.0f - tl[0]) / xlen;
        GLfloat ymid = (1.0f - tl[1]) / ylen;
        aRects.addRect(0.0f, 0.0f,
                       xmid, ymid,
                       tl[0], tl[1],
                       1.0f, 1.0f);
        aRects.addRect(xmid, 0.0f,
                       1.0f, ymid,
                       0.0f, tl[1],
                       br[0], 1.0f);
        aRects.addRect(0.0f, ymid,
                       xmid, 1.0f,
                       tl[0], 0.0f,
                       1.0f, br[1]);
        aRects.addRect(xmid, ymid,
                       1.0f, 1.0f,
                       0.0f, 0.0f,
                       br[0], br[1]);
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
            nsCAutoString log;
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
        nsCAutoString log;
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
        nsCAutoString msg;
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

        bool found = false;
        for (PRUint32 i = 0; i < aArray.Length(); ++i) {
            if (aArray[i].name == name) {
                aArray.RemoveElementAt(i);
                found = true;
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
    for (PRUint32 i = 0; i < aArray.Length(); ++i) {
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
ReportArrayContents(const nsTArray<GLContext::NamedResource>& aArray)
{
    nsTArray<GLContext::NamedResource> copy(aArray);
    copy.Sort();

    GLContext *lastContext = NULL;
    for (PRUint32 i = 0; i < copy.Length(); ++i) {
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
    printf_stderr("== GLContext %p ==\n", this);
    printf_stderr("Outstanding Textures:\n");
    ReportArrayContents(mTrackedTextures);
    printf_stderr("Outstanding Buffers:\n");
    ReportArrayContents(mTrackedBuffers);
    printf_stderr("Outstanding Programs:\n");
    ReportArrayContents(mTrackedPrograms);
    printf_stderr("Outstanding Shaders:\n");
    ReportArrayContents(mTrackedShaders);
    printf_stderr("Outstanding Framebuffers:\n");
    ReportArrayContents(mTrackedFramebuffers);
    printf_stderr("Outstanding Renderbuffers:\n");
    ReportArrayContents(mTrackedRenderbuffers);
}

#endif /* DEBUG */

} /* namespace gl */
} /* namespace mozilla */
