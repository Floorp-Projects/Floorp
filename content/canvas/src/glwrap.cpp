#ifdef C3D_STANDALONE_BUILD
#include "c3d-standalone.h"
#endif

#include <string.h>
#include <stdio.h>

#include "prlink.h"

#include "glwrap.h"

#define MAX_SYMBOL_LENGTH 128
#define MAX_SYMBOL_NAMES 5

#ifdef MOZ_X11
#include <GL/glx.h>
#endif

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

PRFuncPtr
LibrarySymbolLoader::LookupSymbol(const char *sym, bool tryplatform)
{
    PRFuncPtr res = 0;

    // try finding it in the library directly, if we have one
    if (mLibrary) {
        res = PR_FindFunctionSymbol(mLibrary, sym);
    }

    // try finding it in the process
    if (!res) {
        PRLibrary *leakedLibRef;
        res = PR_FindFunctionSymbolAndLibrary(sym, &leakedLibRef);
    }

    // no? then try looking it up via the lookup symbol
    if (!res && tryplatform && mLookupFunc) {
        res = mLookupFunc (sym);
    }

    return res;
}

bool
LibrarySymbolLoader::LoadSymbols(SymLoadStruct *firstStruct, bool tryplatform, const char *prefix)
{
    char sbuf[MAX_SYMBOL_LENGTH * 2];

    SymLoadStruct *ss = firstStruct;
    while (ss->symPointer) {
        *ss->symPointer = 0;

        for (int i = 0; i < MAX_SYMBOL_NAMES; i++) {
            if (ss->symNames[i] == NULL)
                break;

            const char *s = ss->symNames[i];
            if (prefix && *prefix != 0) {
                strcpy(sbuf, prefix);
                strcat(sbuf, ss->symNames[i]);
                s = sbuf;
            }

            PRFuncPtr p = LookupSymbol(s, tryplatform);
            if (p) {
                *ss->symPointer = p;
                break;
            }
        }

        if (*ss->symPointer == 0) {
            fprintf (stderr, "Can't find symbol '%s'\n", ss->symNames[0]);
            return false;
        }

        ss++;
    }

    return true;
}

bool
OSMesaWrap::Init()
{
    if (fCreateContextExt)
        return true;

    SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &fCreateContextExt, { "OSMesaCreateContextExt", NULL } },
        { (PRFuncPtr*) &fMakeCurrent, { "OSMesaMakeCurrent", NULL } },
        { (PRFuncPtr*) &fPixelStore, { "OSMesaPixelStore", NULL } },
        { (PRFuncPtr*) &fDestroyContext, { "OSMesaDestroyContext", NULL } },
        { (PRFuncPtr*) &fGetCurrentContext, { "OSMesaGetCurrentContext", NULL } },
        { (PRFuncPtr*) &fMakeCurrent, { "OSMesaMakeCurrent", NULL } },
        { (PRFuncPtr*) &fGetProcAddress, { "OSMesaGetProcAddress", NULL } },
        { NULL, { NULL } }
    };

    return LoadSymbols(&symbols[0]);
}

bool
GLES20Wrap::Init(NativeGLMode mode)
{
    if (mode & TRY_NATIVE_GL) {
        if (InitNative())
            return true;
    }

    if (mode & TRY_SOFTWARE_GL) {
        if (InitSoftware())
            return true;
    }

    return false;
}

bool
GLES20Wrap::InitNative()
{
    return InitWithPrefix("gl", true);
}

bool
GLES20Wrap::InitSoftware()
{
    return InitWithPrefix("mgl", true);
}

/*
 * XXX - we should really know the ARB/EXT variants of these
 * instead of only handling the symbol if it's exposed directly.
 */

bool
GLES20Wrap::InitWithPrefix(const char *prefix, bool trygl)
{
    if (ok)
        return true;

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
#ifdef USE_GLES2
        { (PRFuncPtr*) &fClearDepthf, { "ClearDepthf", NULL } },
#else
        { (PRFuncPtr*) &fClearDepth, { "ClearDepth", NULL } },
#endif
        { (PRFuncPtr*) &fClearStencil, { "ClearStencil", NULL } },
        { (PRFuncPtr*) &fColorMask, { "ColorMask", NULL } },
        { (PRFuncPtr*) &fCreateProgram, { "CreateProgram", "CreateProgramARB", NULL } },
        { (PRFuncPtr*) &fCreateShader, { "CreateShader", "CreateShaderARB", NULL } },
        { (PRFuncPtr*) &fCullFace, { "CullFace", NULL } },
        { (PRFuncPtr*) &fDeleteBuffers, { "DeleteBuffers", "DeleteBuffersARB", NULL } },
        { (PRFuncPtr*) &fDeleteTextures, { "DeleteTextures", "DeleteTexturesARB", NULL } },
        { (PRFuncPtr*) &fDeleteProgram, { "DeleteProgram", "DeleteProgramARB", NULL } },
        { (PRFuncPtr*) &fDeleteShader, { "DeleteShader", "DeleteShaderARB", NULL } },
        { (PRFuncPtr*) &fDetachShader, { "DetachShader", "DetachShaderARB", NULL } },
        { (PRFuncPtr*) &fDepthFunc, { "DepthFunc", NULL } },
        { (PRFuncPtr*) &fDepthMask, { "DepthMask", NULL } },
#ifdef USE_GLES2
        { (PRFuncPtr*) &fDepthRangef, { "DepthRangef", NULL } },
#else
        { (PRFuncPtr*) &fDepthRange, { "DepthRange", NULL } },
#endif
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
        { (PRFuncPtr*) &fGenBuffers, { "GenBuffers", "GenBuffersARB", NULL } },
        { (PRFuncPtr*) &fGenTextures, { "GenTextures", NULL } },
        { (PRFuncPtr*) &fGetError, { "GetError", NULL } },
        { (PRFuncPtr*) &fGetProgramiv, { "GetProgramiv", "GetProgramivARB", NULL } },
        { (PRFuncPtr*) &fGetProgramInfoLog, { "GetProgramInfoLog", "GetProgramInfoLogARB", NULL } },
        { (PRFuncPtr*) &fTexParameteri, { "TexParameteri", NULL } },
        { (PRFuncPtr*) &fTexParameterf, { "TexParameterf", NULL } },
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
        { (PRFuncPtr*) &fDeleteFramebuffers, { "DeleteFramebuffers", "DeleteFramebuffersEXT", NULL } },
        { (PRFuncPtr*) &fDeleteRenderbuffers, { "DeleteRenderbuffers", "DeleteRenderbuffersEXT", NULL } },
        { (PRFuncPtr*) &fFramebufferRenderbuffer, { "FramebufferRenderbuffer", "FramebufferRenderbufferEXT", NULL } },
        { (PRFuncPtr*) &fFramebufferTexture2D, { "FramebufferTexture2D", "FramebufferTexture2DEXT", NULL } },
        { (PRFuncPtr*) &fGenerateMipmap, { "GenerateMipmap", "GenerateMipmapEXT", NULL } },
        { (PRFuncPtr*) &fGenFramebuffers, { "GenFramebuffers", "GenFramebuffersEXT", NULL } },
        { (PRFuncPtr*) &fGenRenderbuffers, { "GenRenderbuffers", "GenRenderbuffersEXT", NULL } },
        { (PRFuncPtr*) &fGetFramebufferAttachmentParameteriv, { "GetFramebufferAttachmentParameteriv", "GetFramebufferAttachmentParameterivEXT", NULL } },
        { (PRFuncPtr*) &fGetRenderbufferParameteriv, { "GetRenderbufferParameteriv", "GetRenderbufferParameterivEXT", NULL } },
        { (PRFuncPtr*) &fIsFramebuffer, { "IsFramebuffer", "IsFramebufferEXT", NULL } },
        { (PRFuncPtr*) &fIsRenderbuffer, { "IsRenderbuffer", "IsRenderbufferEXT", NULL } },
        { (PRFuncPtr*) &fRenderbufferStorage, { "RenderbufferStorage", "RenderbufferStorageEXT", NULL } },
	{ (PRFuncPtr*) &fMapBuffer, { "MapBuffer", NULL } },
	{ (PRFuncPtr*) &fUnmapBuffer, { "UnmapBuffer", NULL } },

        { NULL, { NULL } },

    };

    ok = LoadSymbols(&symbols[0], trygl, prefix);

    return ok;
}
