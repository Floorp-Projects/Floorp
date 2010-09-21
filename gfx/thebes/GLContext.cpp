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
 *   Mozilla Corporation.
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

#include "nsThreadUtils.h"

#include "gfxImageSurface.h"
#include "GLContext.h"
#include "GLContextProvider.h"

namespace mozilla {
namespace gl {

// define this here since it's global to GLContextProvider, not any
// specific implementation
const ContextFormat ContextFormat::BasicRGBA32Format(ContextFormat::BasicRGBA32);

#define MAX_SYMBOL_LENGTH 128
#define MAX_SYMBOL_NAMES 5

PRBool
LibrarySymbolLoader::OpenLibrary(const char *library)
{
    PRLibSpec lspec;
    lspec.type = PR_LibSpec_Pathname;
    lspec.value.pathname = library;

    mLibrary = PR_LoadLibraryWithFlags(lspec, PR_LD_LAZY | PR_LD_LOCAL);
    if (!mLibrary)
        return PR_FALSE;

    return PR_TRUE;
}

PRBool
LibrarySymbolLoader::LoadSymbols(SymLoadStruct *firstStruct, PRBool tryplatform, const char *prefix)
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

PRBool
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

    return failCount == 0 ? PR_TRUE : PR_FALSE;
}

/*
 * XXX - we should really know the ARB/EXT variants of these
 * instead of only handling the symbol if it's exposed directly.
 */

PRBool
GLContext::InitWithPrefix(const char *prefix, PRBool trygl)
{
    if (mInitialized) {
        return PR_TRUE;
    }

    SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &fActiveTexture, { "ActiveTexture", "ActiveTextureARB", NULL } },
        { (PRFuncPtr*) &fAttachShader, { "AttachShader", "AttachShaderARB", NULL } },
        { (PRFuncPtr*) &fBindAttribLocation, { "BindAttribLocation", "BindAttribLocationARB", NULL } },
        { (PRFuncPtr*) &fBindBuffer, { "BindBuffer", "BindBufferARB", NULL } },
        { (PRFuncPtr*) &fBindTexture, { "BindTexture", "BindTextureARB", NULL } },
        { (PRFuncPtr*) &fBlendColor, { "BlendColor", NULL } },
        { (PRFuncPtr*) &fBlendEquation, { "BlendEquation", NULL } },
        { (PRFuncPtr*) &fBlendEquationSeparate, { "BlendEquationSeparate", "BlendEquationSeparateEXT", NULL } },
        { (PRFuncPtr*) &fBlendFunc, { "BlendFunc", NULL } },
        { (PRFuncPtr*) &fBlendFuncSeparate, { "BlendFuncSeparate", "BlendFuncSeparateEXT", NULL } },
        { (PRFuncPtr*) &fBufferData, { "BufferData", NULL } },
        { (PRFuncPtr*) &fBufferSubData, { "BufferSubData", NULL } },
        { (PRFuncPtr*) &fClear, { "Clear", NULL } },
        { (PRFuncPtr*) &fClearColor, { "ClearColor", NULL } },
        { (PRFuncPtr*) &fClearStencil, { "ClearStencil", NULL } },
        { (PRFuncPtr*) &fColorMask, { "ColorMask", NULL } },
        { (PRFuncPtr*) &fCullFace, { "CullFace", NULL } },
        { (PRFuncPtr*) &fDetachShader, { "DetachShader", "DetachShaderARB", NULL } },
        { (PRFuncPtr*) &fDepthFunc, { "DepthFunc", NULL } },
        { (PRFuncPtr*) &fDepthMask, { "DepthMask", NULL } },
        { (PRFuncPtr*) &fDisable, { "Disable", NULL } },
        { (PRFuncPtr*) &fDisableVertexAttribArray, { "DisableVertexAttribArray", "DisableVertexAttribArrayARB", NULL } },
        { (PRFuncPtr*) &fDrawArrays, { "DrawArrays", NULL } },
        { (PRFuncPtr*) &fDrawElements, { "DrawElements", NULL } },
        { (PRFuncPtr*) &fEnable, { "Enable", NULL } },
        { (PRFuncPtr*) &fEnableVertexAttribArray, { "EnableVertexAttribArray", "EnableVertexAttribArrayARB", NULL } },
        { (PRFuncPtr*) &fFinish, { "Finish", NULL } },
        { (PRFuncPtr*) &fFlush, { "Flush", NULL } },
        { (PRFuncPtr*) &fFrontFace, { "FrontFace", NULL } },
        { (PRFuncPtr*) &fGetActiveAttrib, { "GetActiveAttrib", "GetActiveAttribARB", NULL } },
        { (PRFuncPtr*) &fGetActiveUniform, { "GetActiveUniform", "GetActiveUniformARB", NULL } },
        { (PRFuncPtr*) &fGetAttachedShaders, { "GetAttachedShaders", "GetAttachedShadersARB", NULL } },
        { (PRFuncPtr*) &fGetAttribLocation, { "GetAttribLocation", "GetAttribLocationARB", NULL } },
        { (PRFuncPtr*) &fGetIntegerv, { "GetIntegerv", NULL } },
        { (PRFuncPtr*) &fGetFloatv, { "GetFloatv", NULL } },
        { (PRFuncPtr*) &fGetBooleanv, { "GetBooleanv", NULL } },
        { (PRFuncPtr*) &fGetBufferParameteriv, { "GetBufferParameteriv", "GetBufferParameterivARB", NULL } },
        { (PRFuncPtr*) &fGetError, { "GetError", NULL } },
        { (PRFuncPtr*) &fGetProgramiv, { "GetProgramiv", "GetProgramivARB", NULL } },
        { (PRFuncPtr*) &fGetProgramInfoLog, { "GetProgramInfoLog", "GetProgramInfoLogARB", NULL } },
        { (PRFuncPtr*) &fTexParameteri, { "TexParameteri", NULL } },
        { (PRFuncPtr*) &fTexParameterf, { "TexParameterf", NULL } },
        { (PRFuncPtr*) &fGetString, { "GetString", NULL } },
        { (PRFuncPtr*) &fGetTexParameterfv, { "GetTexParameterfv", NULL } },
        { (PRFuncPtr*) &fGetTexParameteriv, { "GetTexParameteriv", NULL } },
        { (PRFuncPtr*) &fGetUniformfv, { "GetUniformfv", "GetUniformfvARB", NULL } },
        { (PRFuncPtr*) &fGetUniformiv, { "GetUniformiv", "GetUniformivARB", NULL } },
        { (PRFuncPtr*) &fGetUniformLocation, { "GetUniformLocation", "GetUniformLocationARB", NULL } },
        { (PRFuncPtr*) &fGetVertexAttribfv, { "GetVertexAttribfv", "GetVertexAttribfvARB", NULL } },
        { (PRFuncPtr*) &fGetVertexAttribiv, { "GetVertexAttribiv", "GetVertexAttribivARB", NULL } },
        { (PRFuncPtr*) &fHint, { "Hint", NULL } },
        { (PRFuncPtr*) &fIsBuffer, { "IsBuffer", "IsBufferARB", NULL } },
        { (PRFuncPtr*) &fIsEnabled, { "IsEnabled", NULL } },
        { (PRFuncPtr*) &fIsProgram, { "IsProgram", "IsProgramARB", NULL } },
        { (PRFuncPtr*) &fIsShader, { "IsShader", "IsShaderARB", NULL } },
        { (PRFuncPtr*) &fIsTexture, { "IsTexture", "IsTextureARB", NULL } },
        { (PRFuncPtr*) &fLineWidth, { "LineWidth", NULL } },
        { (PRFuncPtr*) &fLinkProgram, { "LinkProgram", "LinkProgramARB", NULL } },
        { (PRFuncPtr*) &fPixelStorei, { "PixelStorei", NULL } },
        { (PRFuncPtr*) &fPolygonOffset, { "PolygonOffset", NULL } },
        { (PRFuncPtr*) &fReadPixels, { "ReadPixels", NULL } },
        { (PRFuncPtr*) &fSampleCoverage, { "SampleCoverage", NULL } },
        { (PRFuncPtr*) &fScissor, { "Scissor", NULL } },
        { (PRFuncPtr*) &fStencilFunc, { "StencilFunc", NULL } },
        { (PRFuncPtr*) &fStencilFuncSeparate, { "StencilFuncSeparate", "StencilFuncSeparateEXT", NULL } },
        { (PRFuncPtr*) &fStencilMask, { "StencilMask", NULL } },
        { (PRFuncPtr*) &fStencilMaskSeparate, { "StencilMaskSeparate", "StencilMaskSeparateEXT", NULL } },
        { (PRFuncPtr*) &fStencilOp, { "StencilOp", NULL } },
        { (PRFuncPtr*) &fStencilOpSeparate, { "StencilOpSeparate", "StencilOpSeparateEXT", NULL } },
        { (PRFuncPtr*) &fTexImage2D, { "TexImage2D", NULL } },
        { (PRFuncPtr*) &fTexSubImage2D, { "TexSubImage2D", NULL } },
        { (PRFuncPtr*) &fUniform1f, { "Uniform1f", NULL } },
        { (PRFuncPtr*) &fUniform1fv, { "Uniform1fv", NULL } },
        { (PRFuncPtr*) &fUniform1i, { "Uniform1i", NULL } },
        { (PRFuncPtr*) &fUniform1iv, { "Uniform1iv", NULL } },
        { (PRFuncPtr*) &fUniform2f, { "Uniform2f", NULL } },
        { (PRFuncPtr*) &fUniform2fv, { "Uniform2fv", NULL } },
        { (PRFuncPtr*) &fUniform2i, { "Uniform2i", NULL } },
        { (PRFuncPtr*) &fUniform2iv, { "Uniform2iv", NULL } },
        { (PRFuncPtr*) &fUniform3f, { "Uniform3f", NULL } },
        { (PRFuncPtr*) &fUniform3fv, { "Uniform3fv", NULL } },
        { (PRFuncPtr*) &fUniform3i, { "Uniform3i", NULL } },
        { (PRFuncPtr*) &fUniform3iv, { "Uniform3iv", NULL } },
        { (PRFuncPtr*) &fUniform4f, { "Uniform4f", NULL } },
        { (PRFuncPtr*) &fUniform4fv, { "Uniform4fv", NULL } },
        { (PRFuncPtr*) &fUniform4i, { "Uniform4i", NULL } },
        { (PRFuncPtr*) &fUniform4iv, { "Uniform4iv", NULL } },
        { (PRFuncPtr*) &fUniformMatrix2fv, { "UniformMatrix2fv", NULL } },
        { (PRFuncPtr*) &fUniformMatrix3fv, { "UniformMatrix3fv", NULL } },
        { (PRFuncPtr*) &fUniformMatrix4fv, { "UniformMatrix4fv", NULL } },
        { (PRFuncPtr*) &fUseProgram, { "UseProgram", NULL } },
        { (PRFuncPtr*) &fValidateProgram, { "ValidateProgram", NULL } },
        { (PRFuncPtr*) &fVertexAttribPointer, { "VertexAttribPointer", NULL } },
        { (PRFuncPtr*) &fVertexAttrib1f, { "VertexAttrib1f", NULL } },
        { (PRFuncPtr*) &fVertexAttrib2f, { "VertexAttrib2f", NULL } },
        { (PRFuncPtr*) &fVertexAttrib3f, { "VertexAttrib3f", NULL } },
        { (PRFuncPtr*) &fVertexAttrib4f, { "VertexAttrib4f", NULL } },
        { (PRFuncPtr*) &fVertexAttrib1fv, { "VertexAttrib1fv", NULL } },
        { (PRFuncPtr*) &fVertexAttrib2fv, { "VertexAttrib2fv", NULL } },
        { (PRFuncPtr*) &fVertexAttrib3fv, { "VertexAttrib3fv", NULL } },
        { (PRFuncPtr*) &fVertexAttrib4fv, { "VertexAttrib4fv", NULL } },
        { (PRFuncPtr*) &fViewport, { "Viewport", NULL } },
        { (PRFuncPtr*) &fCompileShader, { "CompileShader", NULL } },
        { (PRFuncPtr*) &fCopyTexImage2D, { "CopyTexImage2D", NULL } },
        { (PRFuncPtr*) &fCopyTexSubImage2D, { "CopyTexSubImage2D", NULL } },
        { (PRFuncPtr*) &fGetShaderiv, { "GetShaderiv", NULL } },
        { (PRFuncPtr*) &fGetShaderInfoLog, { "GetShaderInfoLog", NULL } },
        { (PRFuncPtr*) &fGetShaderSource, { "GetShaderSource", NULL } },
        { (PRFuncPtr*) &fShaderSource, { "ShaderSource", NULL } },
        { (PRFuncPtr*) &fVertexAttribPointer, { "VertexAttribPointer", NULL } },
        { (PRFuncPtr*) &fBindFramebuffer, { "BindFramebuffer", "BindFramebufferEXT", NULL } },
        { (PRFuncPtr*) &fBindRenderbuffer, { "BindRenderbuffer", "BindRenderbufferEXT", NULL } },
        { (PRFuncPtr*) &fCheckFramebufferStatus, { "CheckFramebufferStatus", "CheckFramebufferStatusEXT", NULL } },
        { (PRFuncPtr*) &fFramebufferRenderbuffer, { "FramebufferRenderbuffer", "FramebufferRenderbufferEXT", NULL } },
        { (PRFuncPtr*) &fFramebufferTexture2D, { "FramebufferTexture2D", "FramebufferTexture2DEXT", NULL } },
        { (PRFuncPtr*) &fGenerateMipmap, { "GenerateMipmap", "GenerateMipmapEXT", NULL } },
        { (PRFuncPtr*) &fGetFramebufferAttachmentParameteriv, { "GetFramebufferAttachmentParameteriv", "GetFramebufferAttachmentParameterivEXT", NULL } },
        { (PRFuncPtr*) &fGetRenderbufferParameteriv, { "GetRenderbufferParameteriv", "GetRenderbufferParameterivEXT", NULL } },
        { (PRFuncPtr*) &fIsFramebuffer, { "IsFramebuffer", "IsFramebufferEXT", NULL } },
        { (PRFuncPtr*) &fIsRenderbuffer, { "IsRenderbuffer", "IsRenderbufferEXT", NULL } },
        { (PRFuncPtr*) &fRenderbufferStorage, { "RenderbufferStorage", "RenderbufferStorageEXT", NULL } },

        { (PRFuncPtr*) &priv_fGenBuffers, { "GenBuffers", "GenBuffersARB", NULL } },
        { (PRFuncPtr*) &priv_fGenTextures, { "GenTextures", NULL } },
        { (PRFuncPtr*) &priv_fCreateProgram, { "CreateProgram", "CreateProgramARB", NULL } },
        { (PRFuncPtr*) &priv_fCreateShader, { "CreateShader", "CreateShaderARB", NULL } },
        { (PRFuncPtr*) &priv_fGenFramebuffers, { "GenFramebuffers", "GenFramebuffersEXT", NULL } },
        { (PRFuncPtr*) &priv_fGenRenderbuffers, { "GenRenderbuffers", "GenRenderbuffersEXT", NULL } },

        { (PRFuncPtr*) &priv_fDeleteBuffers, { "DeleteBuffers", "DeleteBuffersARB", NULL } },
        { (PRFuncPtr*) &priv_fDeleteTextures, { "DeleteTextures", "DeleteTexturesARB", NULL } },
        { (PRFuncPtr*) &priv_fDeleteProgram, { "DeleteProgram", "DeleteProgramARB", NULL } },
        { (PRFuncPtr*) &priv_fDeleteShader, { "DeleteShader", "DeleteShaderARB", NULL } },
        { (PRFuncPtr*) &priv_fDeleteFramebuffers, { "DeleteFramebuffers", "DeleteFramebuffersEXT", NULL } },
        { (PRFuncPtr*) &priv_fDeleteRenderbuffers, { "DeleteRenderbuffers", "DeleteRenderbuffersEXT", NULL } },

        { mIsGLES2 ? (PRFuncPtr*) &priv_fClearDepthf : (PRFuncPtr*) &priv_fClearDepth,
          { mIsGLES2 ? "ClearDepthf" : "ClearDepth", NULL } },
        { mIsGLES2 ? (PRFuncPtr*) &priv_fDepthRangef : (PRFuncPtr*) &priv_fDepthRange,
          { mIsGLES2 ? "DepthRangef" : "DepthRange", NULL } },

        // XXX FIXME -- we shouldn't be using glReadBuffer!
        { mIsGLES2 ? (PRFuncPtr*) NULL : (PRFuncPtr*) &fReadBuffer,
          { mIsGLES2 ? NULL : "ReadBuffer", NULL } },

        { NULL, { NULL } },

    };

    mInitialized = LoadSymbols(&symbols[0], trygl, prefix);

    if (mInitialized) {
        InitExtensions();
    }

    return mInitialized;
}

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
    NULL
};

void
GLContext::InitExtensions()
{
    MakeCurrent();
    const GLubyte *extensions = fGetString(LOCAL_GL_EXTENSIONS);
    char *exts = strdup((char *)extensions);

    static bool once = false;

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

    once = true;
}

PRBool
GLContext::IsExtensionSupported(const char *extension)
{
    const GLubyte *extensions = NULL;
    const GLubyte *start;
    GLubyte *where, *terminator;

    /* Extension names should not have spaces. */
    where = (GLubyte *) strchr(extension, ' ');
    if (where || *extension == '\0')
        return PR_FALSE;

    extensions = fGetString(LOCAL_GL_EXTENSIONS);
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
                return PR_TRUE;
            }
        }
        start = terminator;
    }
    return PR_FALSE;
}

already_AddRefed<TextureImage>
GLContext::CreateTextureImage(const nsIntSize& aSize,
                              TextureImage::ContentType aContentType,
                              GLint aWrapMode,
                              PRBool aUseNearestFilter)
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
  DEBUG_GL_ERROR_CHECK(this);

  return CreateBasicTextureImage(texture, aSize, aContentType, this);
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

gfxContext*
BasicTextureImage::BeginUpdate(nsIntRegion& aRegion)
{
    NS_ASSERTION(!mUpdateContext, "BeginUpdate() without EndUpdate()?");

    // determine the region the client will need to repaint
    if (!mTextureInited)
        // if the texture hasn't been initialized yet, force the
        // client to paint everything
        mUpdateRect = nsIntRect(nsIntPoint(0, 0), mSize);
    else
        mUpdateRect = aRegion.GetBounds();
    // the basic impl can't upload updates to disparate regions,
    // only rects
    aRegion = nsIntRegion(mUpdateRect);
        
    nsIntSize rgnSize = mUpdateRect.Size();
    if (!nsIntRect(nsIntPoint(0, 0), mSize).Contains(mUpdateRect)) {
        NS_ERROR("update outside of image");
        return NULL;
    }

    ImageFormat format =
        (GetContentType() == gfxASurface::CONTENT_COLOR) ?
        gfxASurface::ImageFormatRGB24 : gfxASurface::ImageFormatARGB32;
    nsRefPtr<gfxASurface> updateSurface =
        CreateUpdateSurface(gfxIntSize(rgnSize.width, rgnSize.height),
                            format);
    if (!updateSurface)
        return NULL;

    updateSurface->SetDeviceOffset(gfxPoint(-mUpdateRect.x, -mUpdateRect.y));

    mUpdateContext = new gfxContext(updateSurface);
    return mUpdateContext;
}

PRBool
BasicTextureImage::EndUpdate()
{
    NS_ASSERTION(!!mUpdateContext, "EndUpdate() without BeginUpdate()?");

    // FIXME: this is the slow boat.  Make me fast (with GLXPixmap?).
    nsRefPtr<gfxASurface> originalSurface = mUpdateContext->OriginalSurface();

    // Undo the device offset that BeginUpdate set; doesn't much matter for us here,
    // but important if we ever do anything directly with the surface.
    originalSurface->SetDeviceOffset(gfxPoint(0, 0));

    nsRefPtr<gfxImageSurface> uploadImage = GetImageForUpload(originalSurface);
    if (!uploadImage)
        return PR_FALSE;

    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    if (!mTextureInited)
    {
        mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                                0,
                                LOCAL_GL_RGBA,
                                mUpdateRect.width,
                                mUpdateRect.height,
                                0,
                                LOCAL_GL_RGBA,
                                LOCAL_GL_UNSIGNED_BYTE,
                                uploadImage->Data());
        mTextureInited = PR_TRUE;
    } else {
        mGLContext->fTexSubImage2D(LOCAL_GL_TEXTURE_2D,
                                   0,
                                   mUpdateRect.x,
                                   mUpdateRect.y,
                                   mUpdateRect.width,
                                   mUpdateRect.height,
                                   LOCAL_GL_RGBA,
                                   LOCAL_GL_UNSIGNED_BYTE,
                                   uploadImage->Data());
    }
    mUpdateContext = NULL;
    return PR_TRUE;         // mTexture is bound
}

PRBool
GLContext::ResizeOffscreenFBO(const gfxIntSize& aSize)
{
    MakeCurrent();

    bool alpha = mCreationFormat.alpha > 0;
    int depth = mCreationFormat.depth;
    int stencil = mCreationFormat.stencil;

    bool firstTime = (mOffscreenFBO == 0);

    GLuint curBoundTexture = 0;
    GLuint curBoundRenderbuffer = 0;
    GLuint curBoundFramebuffer = 0;

    GLint viewport[4];

    bool useDepthStencil =
        !mIsGLES2 || IsExtensionSupported(OES_packed_depth_stencil);

    // save a few things for later restoring
    fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, (GLint*) &curBoundTexture);
    fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, (GLint*) &curBoundFramebuffer);
    fGetIntegerv(LOCAL_GL_RENDERBUFFER_BINDING, (GLint*) &curBoundRenderbuffer);
    fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);

    // If this is the first time we're going through this, we need
    // to create the objects we'll use.  Otherwise, just bind them.
    if (firstTime) {
        fGenTextures(1, &mOffscreenTexture);
        fBindTexture(LOCAL_GL_TEXTURE_2D, mOffscreenTexture);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
        fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);

        fGenFramebuffers(1, &mOffscreenFBO);
        fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mOffscreenFBO);

        if (depth && stencil && useDepthStencil) {
            fGenRenderbuffers(1, &mOffscreenDepthRB);
        } else {
            if (depth) {
                fGenRenderbuffers(1, &mOffscreenDepthRB);
            }

            if (stencil) {
                fGenRenderbuffers(1, &mOffscreenStencilRB);
            }
        }
    } else {
        fBindTexture(LOCAL_GL_TEXTURE_2D, mOffscreenTexture);
        fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mOffscreenFBO);
    }

    // resize the FBO components
    if (alpha) {
        fTexImage2D(LOCAL_GL_TEXTURE_2D,
                    0,
                    LOCAL_GL_RGBA,
                    aSize.width, aSize.height,
                    0,
                    LOCAL_GL_RGBA,
                    LOCAL_GL_UNSIGNED_BYTE,
                    NULL);
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
    }

    if (depth && stencil && useDepthStencil) {
        fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mOffscreenDepthRB);
        fRenderbufferStorage(LOCAL_GL_RENDERBUFFER,
                             LOCAL_GL_DEPTH24_STENCIL8,
                             aSize.width, aSize.height);
    } else {
        if (depth) {
            GLenum depthType;
            if (mIsGLES2) {
                if (IsExtensionSupported(OES_depth32)) {
                    depthType = LOCAL_GL_DEPTH_COMPONENT32;
                } else if (IsExtensionSupported(OES_depth24)) {
                    depthType = LOCAL_GL_DEPTH_COMPONENT24;
                } else {
                    depthType = LOCAL_GL_DEPTH_COMPONENT16;
                }
            } else {
                depthType = LOCAL_GL_DEPTH_COMPONENT24;
            }

            fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mOffscreenDepthRB);
            fRenderbufferStorage(LOCAL_GL_RENDERBUFFER,
                                 mIsGLES2 ? LOCAL_GL_DEPTH_COMPONENT16
                                          : LOCAL_GL_DEPTH_COMPONENT24,
                                 aSize.width, aSize.height);
        }

        if (stencil) {
            fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mOffscreenStencilRB);
            fRenderbufferStorage(LOCAL_GL_RENDERBUFFER,
                                 LOCAL_GL_STENCIL_INDEX8,
                                 aSize.width, aSize.height);
        }
    }

    // Now assemble the FBO, if we're creating one
    // for the first time.
    if (firstTime) {
        fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                              LOCAL_GL_COLOR_ATTACHMENT0,
                              LOCAL_GL_TEXTURE_2D,
                              mOffscreenTexture,
                              0);

        if (depth && stencil && useDepthStencil) {
            fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                     LOCAL_GL_DEPTH_ATTACHMENT,
                                     LOCAL_GL_RENDERBUFFER,
                                     mOffscreenDepthRB);
            fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                     LOCAL_GL_STENCIL_ATTACHMENT,
                                     LOCAL_GL_RENDERBUFFER,
                                     mOffscreenDepthRB);
        } else {
            if (depth) {
                fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                         LOCAL_GL_DEPTH_ATTACHMENT,
                                         LOCAL_GL_RENDERBUFFER,
                                         mOffscreenDepthRB);
            }

            if (stencil) {
                fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                         LOCAL_GL_STENCIL_ATTACHMENT,
                                         LOCAL_GL_RENDERBUFFER,
                                         mOffscreenStencilRB);
            }
        }
    }

    // We should be all resized.  Check for framebuffer completeness.
    GLenum status = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        NS_WARNING("Error resizing offscreen framebuffer -- framebuffer not complete");
        return PR_FALSE;
    }

    mOffscreenSize = aSize;
    mOffscreenActualSize = aSize;

    if (firstTime) {
        UpdateActualFormat();

#ifdef DEBUG
        printf_stderr("Created offscreen FBO: r: %d g: %d b: %d a: %d depth: %d stencil: %d\n",
                      mActualFormat.red, mActualFormat.green, mActualFormat.blue, mActualFormat.alpha,
                      mActualFormat.depth, mActualFormat.stencil);
#endif
    }

    // We're good, and the framebuffer is already attached, so let's
    // clear out our new framebuffer; otherwise we'll end up displaying
    // random memory.  We saved all of these things earlier so that we
    // can restore them.
    fViewport(0, 0, aSize.width, aSize.height);

    // Clear the new framebuffer with the full viewport
    ClearSafely();

    // Ok, now restore the GL state back to what it was before the resize took place.
    fBindTexture(LOCAL_GL_TEXTURE_2D, curBoundTexture);
    fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, curBoundRenderbuffer);
    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, curBoundFramebuffer);

    // -don't- restore the viewport the first time through this, since
    // the previous one isn't valid.
    if (!firstTime)
        fViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    return PR_TRUE;
}

void
GLContext::DeleteOffscreenFBO()
{
    fDeleteFramebuffers(1, &mOffscreenFBO);
    fDeleteTextures(1, &mOffscreenTexture);
    fDeleteRenderbuffers(1, &mOffscreenDepthRB);
    fDeleteRenderbuffers(1, &mOffscreenStencilRB);

    mOffscreenFBO = 0;
    mOffscreenTexture = 0;
    mOffscreenDepthRB = 0;
    mOffscreenStencilRB = 0;
}

void
GLContext::ClearSafely()
{
    GLfloat clearColor[4];
    GLfloat clearDepth;
    GLint clearStencil;

    fGetFloatv(LOCAL_GL_COLOR_CLEAR_VALUE, clearColor);
    fGetFloatv(LOCAL_GL_DEPTH_CLEAR_VALUE, &clearDepth);
    fGetIntegerv(LOCAL_GL_STENCIL_CLEAR_VALUE, &clearStencil);

    fClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    fClearStencil(0);
    fClearDepth(1.0f);

    fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);

    fClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    fClearStencil(clearStencil);
    fClearDepth(clearDepth);
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
    MakeCurrent();
    DeleteOffscreenFBO();
    memset(&mFunctionListStartSentinel, 0, &mFunctionListEndSentinel - &mFunctionListStartSentinel);
}

already_AddRefed<gfxImageSurface>
GLContext::ReadTextureImage(GLuint aTexture,
                            const gfxIntSize& aSize,
                            GLenum aTextureFormat)
{
    MakeCurrent();

    nsRefPtr<gfxImageSurface> isurf;

    GLint oldrb, oldfb, oldprog, oldvp[4], oldPackAlignment;
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
    fGetIntegerv(LOCAL_GL_VIEWPORT, oldvp);
    fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, &oldPackAlignment);

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

    fViewport(0, 0, aSize.width, aSize.height);

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

    if (oldPackAlignment != 4)
        fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, oldPackAlignment);

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
    fViewport(oldvp[0], oldvp[1], oldvp[2], oldvp[3]);

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

        if (IsExtensionSupported(gl::GLContext::EXT_read_format_bgra) ||
            IsExtensionSupported(gl::GLContext::IMG_read_format) ||
            IsExtensionSupported(gl::GLContext::EXT_bgra))
        {
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

        PRBool found = PR_FALSE;
        for (PRUint32 i = 0; i < aArray.Length(); ++i) {
            if (aArray[i].name == name) {
                aArray.RemoveElementAt(i);
                found = PR_TRUE;
                break;
            }
        }
#ifdef DEBUG
        if (!found) {
            printf_stderr("GL Context %p deleting resource %d, which doesn't exist!\n", aOrigin, name);
        }
#endif
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
            aArray[i].originDeleted = PR_TRUE;
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
