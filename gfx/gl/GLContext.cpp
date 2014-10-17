/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "GLContext.h"
#include "GLBlitHelper.h"
#include "GLBlitTextureImageHelper.h"
#include "GLReadTexImageHelper.h"

#include "gfxCrashReporterUtils.h"
#include "gfxUtils.h"
#include "GLContextProvider.h"
#include "GLTextureImage.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "prlink.h"
#include "ScopedGLHelpers.h"
#include "SharedSurfaceGL.h"
#include "GfxTexturesReporter.h"
#include "TextureGarbageBin.h"
#include "gfx2DGlue.h"
#include "gfxPrefs.h"
#include "mozilla/IntegerPrintfMacros.h"

#include "OGLShaderProgram.h" // for ShaderProgramType

#include "mozilla/DebugOnly.h"

#ifdef XP_MACOSX
#include <CoreServices/CoreServices.h>
#include "gfxColor.h"
#endif

#if defined(MOZ_WIDGET_COCOA)
#include "nsCocoaFeatures.h"
#endif

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::layers;

#ifdef MOZ_GL_DEBUG
unsigned GLContext::sCurrentGLContextTLS = -1;
#endif

uint32_t GLContext::sDebugMode = 0;


#define MAX_SYMBOL_LENGTH 128
#define MAX_SYMBOL_NAMES 5
#define END_SYMBOLS { nullptr, { nullptr } }

// should match the order of GLExtensions, and be null-terminated.
static const char *sExtensionNames[] = {
    "NO_EXTENSION",
    "GL_AMD_compressed_ATC_texture",
    "GL_ANGLE_depth_texture",
    "GL_ANGLE_framebuffer_blit",
    "GL_ANGLE_framebuffer_multisample",
    "GL_ANGLE_instanced_arrays",
    "GL_ANGLE_texture_compression_dxt3",
    "GL_ANGLE_texture_compression_dxt5",
    "GL_APPLE_client_storage",
    "GL_APPLE_texture_range",
    "GL_APPLE_vertex_array_object",
    "GL_ARB_ES2_compatibility",
    "GL_ARB_ES3_compatibility",
    "GL_ARB_color_buffer_float",
    "GL_ARB_copy_buffer",
    "GL_ARB_depth_texture",
    "GL_ARB_draw_buffers",
    "GL_ARB_draw_instanced",
    "GL_ARB_framebuffer_object",
    "GL_ARB_framebuffer_sRGB",
    "GL_ARB_half_float_pixel",
    "GL_ARB_instanced_arrays",
    "GL_ARB_invalidate_subdata",
    "GL_ARB_map_buffer_range",
    "GL_ARB_occlusion_query2",
    "GL_ARB_pixel_buffer_object",
    "GL_ARB_robustness",
    "GL_ARB_sampler_objects",
    "GL_ARB_sync",
    "GL_ARB_texture_compression",
    "GL_ARB_texture_float",
    "GL_ARB_texture_non_power_of_two",
    "GL_ARB_texture_rectangle",
    "GL_ARB_texture_storage",
    "GL_ARB_transform_feedback2",
    "GL_ARB_uniform_buffer_object",
    "GL_ARB_vertex_array_object",
    "GL_EXT_bgra",
    "GL_EXT_blend_minmax",
    "GL_EXT_color_buffer_float",
    "GL_EXT_color_buffer_half_float",
    "GL_EXT_copy_texture",
    "GL_EXT_draw_buffers",
    "GL_EXT_draw_buffers2",
    "GL_EXT_draw_instanced",
    "GL_EXT_draw_range_elements",
    "GL_EXT_frag_depth",
    "GL_EXT_framebuffer_blit",
    "GL_EXT_framebuffer_multisample",
    "GL_EXT_framebuffer_object",
    "GL_EXT_framebuffer_sRGB",
    "GL_EXT_gpu_shader4",
    "GL_EXT_occlusion_query_boolean",
    "GL_EXT_packed_depth_stencil",
    "GL_EXT_read_format_bgra",
    "GL_EXT_robustness",
    "GL_EXT_sRGB",
    "GL_EXT_shader_texture_lod",
    "GL_EXT_texture3D",
    "GL_EXT_texture_compression_dxt1",
    "GL_EXT_texture_compression_s3tc",
    "GL_EXT_texture_filter_anisotropic",
    "GL_EXT_texture_format_BGRA8888",
    "GL_EXT_texture_sRGB",
    "GL_EXT_texture_storage",
    "GL_EXT_transform_feedback",
    "GL_EXT_unpack_subimage",
    "GL_IMG_read_format",
    "GL_IMG_texture_compression_pvrtc",
    "GL_IMG_texture_npot",
    "GL_KHR_debug",
    "GL_NV_draw_instanced",
    "GL_NV_fence",
    "GL_NV_half_float",
    "GL_NV_instanced_arrays",
    "GL_NV_transform_feedback",
    "GL_NV_transform_feedback2",
    "GL_OES_EGL_image",
    "GL_OES_EGL_image_external",
    "GL_OES_EGL_sync",
    "GL_OES_compressed_ETC1_RGB8_texture",
    "GL_OES_depth24",
    "GL_OES_depth32",
    "GL_OES_depth_texture",
    "GL_OES_element_index_uint",
    "GL_OES_packed_depth_stencil",
    "GL_OES_rgb8_rgba8",
    "GL_OES_standard_derivatives",
    "GL_OES_stencil8",
    "GL_OES_texture_3D",
    "GL_OES_texture_float",
    "GL_OES_texture_float_linear",
    "GL_OES_texture_half_float",
    "GL_OES_texture_half_float_linear",
    "GL_OES_texture_npot",
    "GL_OES_vertex_array_object",
    nullptr
};

static bool
ParseGLVersion(GLContext* gl, unsigned int* version)
{
    GLenum error = gl->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        MOZ_ASSERT(false, "An OpenGL error has been triggered before.");
        return false;
    }

    /**
     * B2G emulator bug work around: The emulator implements OpenGL ES 2.0 on
     * OpenGL 3.2. The bug is that GetIntegerv(LOCAL_GL_{MAJOR,MINOR}_VERSION)
     * returns OpenGL 3.2 instead of generating an error.
     */
    if (!gl->IsGLES())
    {
        /**
         * OpenGL 3.1 and OpenGL ES 3.0 both introduce GL_{MAJOR,MINOR}_VERSION
         * with GetIntegerv. So we first try those constants even though we
         * might not have an OpenGL context supporting them, has this is a
         * better way than parsing GL_VERSION.
         */
        GLint majorVersion = 0;
        GLint minorVersion = 0;

        gl->fGetIntegerv(LOCAL_GL_MAJOR_VERSION, &majorVersion);
        gl->fGetIntegerv(LOCAL_GL_MINOR_VERSION, &minorVersion);

        // If it's not an OpenGL (ES) 3.0 context, we will have an error
        error = gl->fGetError();
        while (gl->fGetError() != LOCAL_GL_NO_ERROR);

        if (error == LOCAL_GL_NO_ERROR &&
            majorVersion > 0 &&
            minorVersion >= 0)
        {
            *version = majorVersion * 100 + minorVersion * 10;
            return true;
        }
    }

    /**
     * We were not able to use GL_{MAJOR,MINOR}_VERSION, so we parse
     * GL_VERSION.
     *
     *
     * OpenGL 2.x, 3.x, 4.x specifications:
     *  The VERSION and SHADING_LANGUAGE_VERSION strings are laid out as follows:
     *
     *    <version number><space><vendor-specific information>
     *
     *  The version number is either of the form major_number.minor_number or
     *  major_number.minor_number.release_number, where the numbers all have
     *  one or more digits.
     *
     *
     * OpenGL ES 2.0, 3.0 specifications:
     *  The VERSION string is laid out as follows:
     *
     *     "OpenGL ES N.M vendor-specific information"
     *
     *  The version number is either of the form major_number.minor_number or
     *  major_number.minor_number.release_number, where the numbers all have
     *  one or more digits.
     *
     *
     * Note:
     *  We don't care about release_number.
     */
    const char* versionString = (const char*)gl->fGetString(LOCAL_GL_VERSION);

    error = gl->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        MOZ_ASSERT(false, "glGetString(GL_VERSION) has generated an error");
        return false;
    } else if (!versionString) {
        MOZ_ASSERT(false, "glGetString(GL_VERSION) has returned 0");
        return false;
    }

    const char kGLESVersionPrefix[] = "OpenGL ES ";
    if (strncmp(versionString, kGLESVersionPrefix, strlen(kGLESVersionPrefix)) == 0) {
        versionString += strlen(kGLESVersionPrefix);
    }

    const char* itr = versionString;
    char* end = nullptr;
    int majorVersion = (int)strtol(itr, &end, 10);

    if (!end) {
        MOZ_ASSERT(false, "Failed to parse the GL major version number.");
        return false;
    } else if (*end != '.') {
        MOZ_ASSERT(false, "Failed to parse GL's major-minor version number separator.");
        return false;
    }

    // we skip the '.' between the major and the minor version
    itr = end + 1;

    end = nullptr;

    int minorVersion = (int)strtol(itr, &end, 10);
    if (!end) {
        MOZ_ASSERT(false, "Failed to parse GL's minor version number.");
        return false;
    }

    if (majorVersion <= 0 || majorVersion >= 100) {
        MOZ_ASSERT(false, "Invalid major version.");
        return false;
    } else if (minorVersion < 0 || minorVersion >= 10) {
        MOZ_ASSERT(false, "Invalid minor version.");
        return false;
    }

    *version = (unsigned int)(majorVersion * 100 + minorVersion * 10);
    return true;
}

GLContext::GLContext(const SurfaceCaps& caps,
          GLContext* sharedContext,
          bool isOffscreen)
  : mInitialized(false),
    mIsOffscreen(isOffscreen),
    mContextLost(false),
    mVersion(0),
    mProfile(ContextProfile::Unknown),
    mVendor(GLVendor::Other),
    mRenderer(GLRenderer::Other),
    mHasRobustness(false),
#ifdef MOZ_GL_DEBUG
    mIsInLocalErrorCheck(false),
#endif
    mSharedContext(sharedContext),
    mCaps(caps),
    mScreen(nullptr),
    mLockedSurface(nullptr),
    mMaxTextureSize(0),
    mMaxCubeMapTextureSize(0),
    mMaxTextureImageSize(0),
    mMaxRenderbufferSize(0),
    mNeedsTextureSizeChecks(false),
    mWorkAroundDriverBugs(true),
    mHeavyGLCallsSinceLastFlush(false)
{
    mOwningThreadId = PlatformThread::CurrentId();
}

GLContext::~GLContext() {
    NS_ASSERTION(IsDestroyed(), "GLContext implementation must call MarkDestroyed in destructor!");
#ifdef MOZ_GL_DEBUG
    if (mSharedContext) {
        GLContext *tip = mSharedContext;
        while (tip->mSharedContext)
            tip = tip->mSharedContext;
        tip->SharedContextDestroyed(this);
        tip->ReportOutstandingNames();
    } else {
        ReportOutstandingNames();
    }
#endif
}

/*static*/ void
GLContext::StaticDebugCallback(GLenum source,
                               GLenum type,
                               GLuint id,
                               GLenum severity,
                               GLsizei length,
                               const GLchar* message,
                               const GLvoid* userParam)
{
    GLContext* gl = (GLContext*)userParam;
    gl->DebugCallback(source, type, id, severity, length, message);
}

static void
ClearSymbols(GLLibraryLoader::SymLoadStruct *symbols)
{
    while (symbols->symPointer) {
        *symbols->symPointer = nullptr;
        symbols++;
    }
}

bool
GLContext::InitWithPrefix(const char *prefix, bool trygl)
{
    ScopedGfxFeatureReporter reporter("GL Context");

    if (mInitialized) {
        reporter.SetSuccessful();
        return true;
    }

    mWorkAroundDriverBugs = gfxPrefs::WorkAroundDriverBugs();

    SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &mSymbols.fActiveTexture, { "ActiveTexture", "ActiveTextureARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fAttachShader, { "AttachShader", "AttachShaderARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBindAttribLocation, { "BindAttribLocation", "BindAttribLocationARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBindBuffer, { "BindBuffer", "BindBufferARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBindTexture, { "BindTexture", "BindTextureARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBlendColor, { "BlendColor", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBlendEquation, { "BlendEquation", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBlendEquationSeparate, { "BlendEquationSeparate", "BlendEquationSeparateEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBlendFunc, { "BlendFunc", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBlendFuncSeparate, { "BlendFuncSeparate", "BlendFuncSeparateEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBufferData, { "BufferData", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBufferSubData, { "BufferSubData", nullptr } },
        { (PRFuncPtr*) &mSymbols.fClear, { "Clear", nullptr } },
        { (PRFuncPtr*) &mSymbols.fClearColor, { "ClearColor", nullptr } },
        { (PRFuncPtr*) &mSymbols.fClearStencil, { "ClearStencil", nullptr } },
        { (PRFuncPtr*) &mSymbols.fColorMask, { "ColorMask", nullptr } },
        { (PRFuncPtr*) &mSymbols.fCompressedTexImage2D, {"CompressedTexImage2D", nullptr} },
        { (PRFuncPtr*) &mSymbols.fCompressedTexSubImage2D, {"CompressedTexSubImage2D", nullptr} },
        { (PRFuncPtr*) &mSymbols.fCullFace, { "CullFace", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDetachShader, { "DetachShader", "DetachShaderARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDepthFunc, { "DepthFunc", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDepthMask, { "DepthMask", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDisable, { "Disable", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDisableVertexAttribArray, { "DisableVertexAttribArray", "DisableVertexAttribArrayARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDrawArrays, { "DrawArrays", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDrawElements, { "DrawElements", nullptr } },
        { (PRFuncPtr*) &mSymbols.fEnable, { "Enable", nullptr } },
        { (PRFuncPtr*) &mSymbols.fEnableVertexAttribArray, { "EnableVertexAttribArray", "EnableVertexAttribArrayARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fFinish, { "Finish", nullptr } },
        { (PRFuncPtr*) &mSymbols.fFlush, { "Flush", nullptr } },
        { (PRFuncPtr*) &mSymbols.fFrontFace, { "FrontFace", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetActiveAttrib, { "GetActiveAttrib", "GetActiveAttribARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetActiveUniform, { "GetActiveUniform", "GetActiveUniformARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetAttachedShaders, { "GetAttachedShaders", "GetAttachedShadersARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetAttribLocation, { "GetAttribLocation", "GetAttribLocationARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetIntegerv, { "GetIntegerv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetFloatv, { "GetFloatv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetBooleanv, { "GetBooleanv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetBufferParameteriv, { "GetBufferParameteriv", "GetBufferParameterivARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetError, { "GetError", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetProgramiv, { "GetProgramiv", "GetProgramivARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetProgramInfoLog, { "GetProgramInfoLog", "GetProgramInfoLogARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fTexParameteri, { "TexParameteri", nullptr } },
        { (PRFuncPtr*) &mSymbols.fTexParameteriv, { "TexParameteriv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fTexParameterf, { "TexParameterf", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetString, { "GetString", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetTexParameterfv, { "GetTexParameterfv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetTexParameteriv, { "GetTexParameteriv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetUniformfv, { "GetUniformfv", "GetUniformfvARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetUniformiv, { "GetUniformiv", "GetUniformivARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetUniformLocation, { "GetUniformLocation", "GetUniformLocationARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetVertexAttribfv, { "GetVertexAttribfv", "GetVertexAttribfvARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetVertexAttribiv, { "GetVertexAttribiv", "GetVertexAttribivARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetVertexAttribPointerv, { "GetVertexAttribPointerv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fHint, { "Hint", nullptr } },
        { (PRFuncPtr*) &mSymbols.fIsBuffer, { "IsBuffer", "IsBufferARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fIsEnabled, { "IsEnabled", nullptr } },
        { (PRFuncPtr*) &mSymbols.fIsProgram, { "IsProgram", "IsProgramARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fIsShader, { "IsShader", "IsShaderARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fIsTexture, { "IsTexture", "IsTextureARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fLineWidth, { "LineWidth", nullptr } },
        { (PRFuncPtr*) &mSymbols.fLinkProgram, { "LinkProgram", "LinkProgramARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fPixelStorei, { "PixelStorei", nullptr } },
        { (PRFuncPtr*) &mSymbols.fPolygonOffset, { "PolygonOffset", nullptr } },
        { (PRFuncPtr*) &mSymbols.fReadPixels, { "ReadPixels", nullptr } },
        { (PRFuncPtr*) &mSymbols.fSampleCoverage, { "SampleCoverage", nullptr } },
        { (PRFuncPtr*) &mSymbols.fScissor, { "Scissor", nullptr } },
        { (PRFuncPtr*) &mSymbols.fStencilFunc, { "StencilFunc", nullptr } },
        { (PRFuncPtr*) &mSymbols.fStencilFuncSeparate, { "StencilFuncSeparate", "StencilFuncSeparateEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fStencilMask, { "StencilMask", nullptr } },
        { (PRFuncPtr*) &mSymbols.fStencilMaskSeparate, { "StencilMaskSeparate", "StencilMaskSeparateEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fStencilOp, { "StencilOp", nullptr } },
        { (PRFuncPtr*) &mSymbols.fStencilOpSeparate, { "StencilOpSeparate", "StencilOpSeparateEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fTexImage2D, { "TexImage2D", nullptr } },
        { (PRFuncPtr*) &mSymbols.fTexSubImage2D, { "TexSubImage2D", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform1f, { "Uniform1f", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform1fv, { "Uniform1fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform1i, { "Uniform1i", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform1iv, { "Uniform1iv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform2f, { "Uniform2f", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform2fv, { "Uniform2fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform2i, { "Uniform2i", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform2iv, { "Uniform2iv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform3f, { "Uniform3f", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform3fv, { "Uniform3fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform3i, { "Uniform3i", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform3iv, { "Uniform3iv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform4f, { "Uniform4f", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform4fv, { "Uniform4fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform4i, { "Uniform4i", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniform4iv, { "Uniform4iv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniformMatrix2fv, { "UniformMatrix2fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniformMatrix3fv, { "UniformMatrix3fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUniformMatrix4fv, { "UniformMatrix4fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fUseProgram, { "UseProgram", nullptr } },
        { (PRFuncPtr*) &mSymbols.fValidateProgram, { "ValidateProgram", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttribPointer, { "VertexAttribPointer", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib1f, { "VertexAttrib1f", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib2f, { "VertexAttrib2f", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib3f, { "VertexAttrib3f", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib4f, { "VertexAttrib4f", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib1fv, { "VertexAttrib1fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib2fv, { "VertexAttrib2fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib3fv, { "VertexAttrib3fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib4fv, { "VertexAttrib4fv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fViewport, { "Viewport", nullptr } },
        { (PRFuncPtr*) &mSymbols.fCompileShader, { "CompileShader", nullptr } },
        { (PRFuncPtr*) &mSymbols.fCopyTexImage2D, { "CopyTexImage2D", nullptr } },
        { (PRFuncPtr*) &mSymbols.fCopyTexSubImage2D, { "CopyTexSubImage2D", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetShaderiv, { "GetShaderiv", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetShaderInfoLog, { "GetShaderInfoLog", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetShaderSource, { "GetShaderSource", nullptr } },
        { (PRFuncPtr*) &mSymbols.fShaderSource, { "ShaderSource", nullptr } },
        { (PRFuncPtr*) &mSymbols.fVertexAttribPointer, { "VertexAttribPointer", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBindFramebuffer, { "BindFramebuffer", "BindFramebufferEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fBindRenderbuffer, { "BindRenderbuffer", "BindRenderbufferEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fCheckFramebufferStatus, { "CheckFramebufferStatus", "CheckFramebufferStatusEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fFramebufferRenderbuffer, { "FramebufferRenderbuffer", "FramebufferRenderbufferEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fFramebufferTexture2D, { "FramebufferTexture2D", "FramebufferTexture2DEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGenerateMipmap, { "GenerateMipmap", "GenerateMipmapEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetFramebufferAttachmentParameteriv, { "GetFramebufferAttachmentParameteriv", "GetFramebufferAttachmentParameterivEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGetRenderbufferParameteriv, { "GetRenderbufferParameteriv", "GetRenderbufferParameterivEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fIsFramebuffer, { "IsFramebuffer", "IsFramebufferEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fIsRenderbuffer, { "IsRenderbuffer", "IsRenderbufferEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fRenderbufferStorage, { "RenderbufferStorage", "RenderbufferStorageEXT", nullptr } },

        { (PRFuncPtr*) &mSymbols.fGenBuffers, { "GenBuffers", "GenBuffersARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGenTextures, { "GenTextures", nullptr } },
        { (PRFuncPtr*) &mSymbols.fCreateProgram, { "CreateProgram", "CreateProgramARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fCreateShader, { "CreateShader", "CreateShaderARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGenFramebuffers, { "GenFramebuffers", "GenFramebuffersEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGenRenderbuffers, { "GenRenderbuffers", "GenRenderbuffersEXT", nullptr } },

        { (PRFuncPtr*) &mSymbols.fDeleteBuffers, { "DeleteBuffers", "DeleteBuffersARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDeleteTextures, { "DeleteTextures", "DeleteTexturesARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDeleteProgram, { "DeleteProgram", "DeleteProgramARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDeleteShader, { "DeleteShader", "DeleteShaderARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDeleteFramebuffers, { "DeleteFramebuffers", "DeleteFramebuffersEXT", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDeleteRenderbuffers, { "DeleteRenderbuffers", "DeleteRenderbuffersEXT", nullptr } },

        END_SYMBOLS

    };

    mInitialized = LoadSymbols(&symbols[0], trygl, prefix);
    MakeCurrent();
    if (mInitialized) {
        unsigned int version = 0;

        ParseGLVersion(this, &version);

#ifdef MOZ_GL_DEBUG
        printf_stderr("OpenGL version detected: %u\n", version);
        printf_stderr("OpenGL vendor: %s\n", fGetString(LOCAL_GL_VENDOR));
        printf_stderr("OpenGL renderer: %s\n", fGetString(LOCAL_GL_RENDERER));
#endif

        if (version >= mVersion) {
            mVersion = version;
        }
        // Don't fail if version < mVersion, see bug 999445,
        // Mac OSX 10.6/10.7 machines with Intel GPUs claim only OpenGL 1.4 but
        // have all the GL2+ extensions that we need.
    }

    // Load OpenGL ES 2.0 symbols, or desktop if we aren't using ES 2.
    if (mInitialized) {
        if (IsGLES()) {
            SymLoadStruct symbols_ES2[] = {
                { (PRFuncPtr*) &mSymbols.fGetShaderPrecisionFormat, { "GetShaderPrecisionFormat", nullptr } },
                { (PRFuncPtr*) &mSymbols.fClearDepthf, { "ClearDepthf", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDepthRangef, { "DepthRangef", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(&symbols_ES2[0], trygl, prefix)) {
                NS_ERROR("OpenGL ES 2.0 supported, but symbols could not be loaded.");
                mInitialized = false;
            }
        } else {
            SymLoadStruct symbols_desktop[] = {
                { (PRFuncPtr*) &mSymbols.fClearDepth, { "ClearDepth", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDepthRange, { "DepthRange", nullptr } },
                { (PRFuncPtr*) &mSymbols.fReadBuffer, { "ReadBuffer", nullptr } },
                { (PRFuncPtr*) &mSymbols.fMapBuffer, { "MapBuffer", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUnmapBuffer, { "UnmapBuffer", nullptr } },
                { (PRFuncPtr*) &mSymbols.fPointParameterf, { "PointParameterf", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDrawBuffer, { "DrawBuffer", nullptr } },
                    // These functions are only used by Skia/GL in desktop mode.
                    // Other parts of Gecko should avoid using these
                    { (PRFuncPtr*) &mSymbols.fDrawBuffers, { "DrawBuffers", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fClientActiveTexture, { "ClientActiveTexture", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fDisableClientState, { "DisableClientState", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fEnableClientState, { "EnableClientState", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fLoadIdentity, { "LoadIdentity", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fLoadMatrixf, { "LoadMatrixf", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fMatrixMode, { "MatrixMode", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fTexGeni, { "TexGeni", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fTexGenf, { "TexGenf", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fTexGenfv, { "TexGenfv", nullptr } },
                    { (PRFuncPtr*) &mSymbols.fVertexPointer, { "VertexPointer", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(&symbols_desktop[0], trygl, prefix)) {
                NS_ERROR("Desktop symbols failed to load.");
                mInitialized = false;
            }
        }
    }

    const char *glVendorString = nullptr;
    const char *glRendererString = nullptr;

    if (mInitialized) {
        // The order of these strings must match up with the order of the enum
        // defined in GLContext.h for vendor IDs
        glVendorString = (const char *)fGetString(LOCAL_GL_VENDOR);
        if (!glVendorString)
            mInitialized = false;

        const char *vendorMatchStrings[size_t(GLVendor::Other)] = {
                "Intel",
                "NVIDIA",
                "ATI",
                "Qualcomm",
                "Imagination",
                "nouveau",
                "Vivante",
                "VMware, Inc."
        };

        mVendor = GLVendor::Other;
        for (size_t i = 0; i < size_t(GLVendor::Other); ++i) {
            if (DoesStringMatch(glVendorString, vendorMatchStrings[i])) {
                mVendor = GLVendor(i);
                break;
            }
        }

        // The order of these strings must match up with the order of the enum
        // defined in GLContext.h for renderer IDs
        glRendererString = (const char *)fGetString(LOCAL_GL_RENDERER);
        if (!glRendererString)
            mInitialized = false;

        const char *rendererMatchStrings[size_t(GLRenderer::Other)] = {
                "Adreno 200",
                "Adreno 205",
                "Adreno (TM) 200",
                "Adreno (TM) 205",
                "Adreno (TM) 320",
                "PowerVR SGX 530",
                "PowerVR SGX 540",
                "NVIDIA Tegra",
                "Android Emulator",
                "Gallium 0.4 on llvmpipe",
                "Intel HD Graphics 3000 OpenGL Engine",
                "Microsoft Basic Render Driver"
        };

        mRenderer = GLRenderer::Other;
        for (size_t i = 0; i < size_t(GLRenderer::Other); ++i) {
            if (DoesStringMatch(glRendererString, rendererMatchStrings[i])) {
                mRenderer = GLRenderer(i);
                break;
            }
        }
    }


#ifdef MOZ_GL_DEBUG
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
#ifdef MOZ_GL_DEBUG
        static bool firstRun = true;
        if (firstRun && DebugMode()) {
            const char *vendors[size_t(GLVendor::Other)] = {
                "Intel",
                "NVIDIA",
                "ATI",
                "Qualcomm"
            };

            MOZ_ASSERT(glVendorString);
            if (mVendor < GLVendor::Other) {
                printf_stderr("OpenGL vendor ('%s') recognized as: %s\n",
                              glVendorString, vendors[size_t(mVendor)]);
            } else {
                printf_stderr("OpenGL vendor ('%s') unrecognized\n", glVendorString);
            }
        }
        firstRun = false;
#endif

        InitExtensions();
        InitFeatures();

        // Disable extensions with partial or incorrect support.
        if (WorkAroundDriverBugs()) {
            if (Renderer() == GLRenderer::AdrenoTM320) {
                MarkUnsupported(GLFeature::standard_derivatives);
            }

            if (Vendor() == GLVendor::Vivante) {
                // bug 958256
                MarkUnsupported(GLFeature::standard_derivatives);
            }

            if (Vendor() == GLVendor::Imagination &&
                Renderer() == GLRenderer::SGX540) {
                // Bug 980048
                MarkExtensionUnsupported(OES_EGL_sync);
            }

            if (Renderer() == GLRenderer::MicrosoftBasicRenderDriver) {
                // Bug 978966: on Microsoft's "Basic Render Driver" (software renderer)
                // multisampling hardcodes blending with the default blendfunc, which breaks WebGL.
                MarkUnsupported(GLFeature::framebuffer_multisample);
            }

#ifdef XP_MACOSX
            // The Mac Nvidia driver, for versions up to and including 10.8, don't seem
            // to properly support this.  See 814839
            // this has been fixed in Mac OS X 10.9. See 907946
            if (Vendor() == gl::GLVendor::NVIDIA &&
                !nsCocoaFeatures::OnMavericksOrLater())
            {
                MarkUnsupported(GLFeature::depth_texture);
            }
#endif
        }

        NS_ASSERTION(!IsExtensionSupported(GLContext::ARB_pixel_buffer_object) ||
                     (mSymbols.fMapBuffer && mSymbols.fUnmapBuffer),
                     "ARB_pixel_buffer_object supported without glMapBuffer/UnmapBuffer being available!");

        if (SupportsRobustness()) {
            mHasRobustness = false;

            if (IsExtensionSupported(ARB_robustness)) {
                SymLoadStruct robustnessSymbols[] = {
                    { (PRFuncPtr*) &mSymbols.fGetGraphicsResetStatus, { "GetGraphicsResetStatusARB", nullptr } },
                    END_SYMBOLS
                };

                if (!LoadSymbols(&robustnessSymbols[0], trygl, prefix)) {
                    NS_ERROR("GL supports ARB_robustness without supplying GetGraphicsResetStatusARB.");

                    mSymbols.fGetGraphicsResetStatus = nullptr;
                } else {
                    mHasRobustness = true;
                }
            }
            if (!IsExtensionSupported(ARB_robustness) &&
                IsExtensionSupported(EXT_robustness)) {
                SymLoadStruct robustnessSymbols[] = {
                    { (PRFuncPtr*) &mSymbols.fGetGraphicsResetStatus, { "GetGraphicsResetStatusEXT", nullptr } },
                    END_SYMBOLS
                };

                if (!LoadSymbols(&robustnessSymbols[0], trygl, prefix)) {
                    NS_ERROR("GL supports EXT_robustness without supplying GetGraphicsResetStatusEXT.");

                    mSymbols.fGetGraphicsResetStatus = nullptr;
                } else {
                    mHasRobustness = true;
                }
            }

            if (!mHasRobustness) {
                MarkUnsupported(GLFeature::robustness);
            }
        }

        // Check for aux symbols based on extensions
        if (IsSupported(GLFeature::framebuffer_blit))
        {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fBlitFramebuffer, { "BlitFramebuffer", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fBlitFramebuffer, { "BlitFramebufferEXT", "BlitFramebufferANGLE", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::framebuffer_blit);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports framebuffer_blit without supplying glBlitFramebuffer");

                MarkUnsupported(GLFeature::framebuffer_blit);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::framebuffer_multisample))
        {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fRenderbufferStorageMultisample, { "RenderbufferStorageMultisample", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fRenderbufferStorageMultisample, { "RenderbufferStorageMultisampleEXT", "RenderbufferStorageMultisampleANGLE", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::framebuffer_multisample);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports framebuffer_multisample without supplying glRenderbufferStorageMultisample");

                MarkUnsupported(GLFeature::framebuffer_multisample);
                ClearSymbols(coreSymbols);
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
                END_SYMBOLS
            };

            if (!LoadSymbols(&syncSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports ARB_sync without supplying its functions.");

                MarkExtensionUnsupported(ARB_sync);
                ClearSymbols(syncSymbols);
            }
        }

        if (IsExtensionSupported(OES_EGL_image)) {
            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fEGLImageTargetTexture2D, { "EGLImageTargetTexture2DOES", nullptr } },
                { (PRFuncPtr*) &mSymbols.fEGLImageTargetRenderbufferStorage, { "EGLImageTargetRenderbufferStorageOES", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports OES_EGL_image without supplying its functions.");

                MarkExtensionUnsupported(OES_EGL_image);
                ClearSymbols(extSymbols);
            }
        }

        if (IsExtensionSupported(APPLE_texture_range)) {
            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fTextureRangeAPPLE, { "TextureRangeAPPLE", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports APPLE_texture_range without supplying its functions.");

                ClearSymbols(extSymbols);
            }
        }

        if (IsSupported(GLFeature::vertex_array_object)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fIsVertexArray, { "IsVertexArray", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGenVertexArrays, { "GenVertexArrays", nullptr } },
                { (PRFuncPtr*) &mSymbols.fBindVertexArray, { "BindVertexArray", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDeleteVertexArrays, { "DeleteVertexArrays", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fIsVertexArray, { "IsVertexArrayARB", "IsVertexArrayOES", "IsVertexArrayAPPLE", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGenVertexArrays, { "GenVertexArraysARB", "GenVertexArraysOES", "GenVertexArraysAPPLE", nullptr } },
                { (PRFuncPtr*) &mSymbols.fBindVertexArray, { "BindVertexArrayARB", "BindVertexArrayOES", "BindVertexArrayAPPLE", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDeleteVertexArrays, { "DeleteVertexArraysARB", "DeleteVertexArraysOES", "DeleteVertexArraysAPPLE", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::vertex_array_object);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports Vertex Array Object without supplying its functions.");

                MarkUnsupported(GLFeature::vertex_array_object);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::draw_instanced)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fDrawArraysInstanced, { "DrawArraysInstanced", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDrawElementsInstanced, { "DrawElementsInstanced", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fDrawArraysInstanced, { "DrawArraysInstancedARB", "DrawArraysInstancedEXT", "DrawArraysInstancedNV", "DrawArraysInstancedANGLE", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDrawElementsInstanced, { "DrawElementsInstancedARB", "DrawElementsInstancedEXT", "DrawElementsInstancedNV", "DrawElementsInstancedANGLE", nullptr }
                },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::draw_instanced);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports instanced draws without supplying its functions.");

                MarkUnsupported(GLFeature::draw_instanced);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::instanced_arrays)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fVertexAttribDivisor, { "VertexAttribDivisor", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fVertexAttribDivisor, { "VertexAttribDivisorARB", "VertexAttribDivisorNV", "VertexAttribDivisorANGLE", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::instanced_arrays);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports array instanced without supplying it function.");

                MarkUnsupported(GLFeature::instanced_arrays);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::texture_storage)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fTexStorage2D, { "TexStorage2D", nullptr } },
                { (PRFuncPtr*) &mSymbols.fTexStorage3D, { "TexStorage3D", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fTexStorage2D, { "TexStorage2DEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fTexStorage3D, { "TexStorage3DEXT", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::texture_storage);
            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports texture storage without supplying its functions.");

                MarkUnsupported(GLFeature::texture_storage);
                MarkExtensionSupported(useCore ? ARB_texture_storage : EXT_texture_storage);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::sampler_objects)) {
            SymLoadStruct samplerObjectsSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGenSamplers, { "GenSamplers", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDeleteSamplers, { "DeleteSamplers", nullptr } },
                { (PRFuncPtr*) &mSymbols.fIsSampler, { "IsSampler", nullptr } },
                { (PRFuncPtr*) &mSymbols.fBindSampler, { "BindSampler", nullptr } },
                { (PRFuncPtr*) &mSymbols.fSamplerParameteri, { "SamplerParameteri", nullptr } },
                { (PRFuncPtr*) &mSymbols.fSamplerParameteriv, { "SamplerParameteriv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fSamplerParameterf, { "SamplerParameterf", nullptr } },
                { (PRFuncPtr*) &mSymbols.fSamplerParameterfv, { "SamplerParameterfv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetSamplerParameteriv, { "GetSamplerParameteriv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetSamplerParameterfv, { "GetSamplerParameterfv", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(samplerObjectsSymbols, trygl, prefix)) {
                NS_ERROR("GL supports sampler objects without supplying its functions.");

                MarkUnsupported(GLFeature::sampler_objects);
                ClearSymbols(samplerObjectsSymbols);
            }
        }

        if (IsSupported(GLFeature::texture_storage)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fTexStorage2D, { "TexStorage2D", nullptr } },
                { (PRFuncPtr*) &mSymbols.fTexStorage3D, { "TexStorage3D", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(coreSymbols, trygl, prefix)) {
                NS_ERROR("GL supports texture storage without supplying its functions.");

                MarkUnsupported(GLFeature::texture_storage);
                MarkExtensionUnsupported(ARB_texture_storage);
                ClearSymbols(coreSymbols);
            }
        }

        // ARB_transform_feedback2/NV_transform_feedback2 is a
        // superset of EXT_transform_feedback/NV_transform_feedback
        // and adds glPauseTransformFeedback &
        // glResumeTransformFeedback, which are required for WebGL2.
        if (IsSupported(GLFeature::transform_feedback2)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fBindBufferBase, { "BindBufferBase", nullptr } },
                { (PRFuncPtr*) &mSymbols.fBindBufferRange, { "BindBufferRange", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGenTransformFeedbacks, { "GenTransformFeedbacks", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDeleteTransformFeedbacks, { "DeleteTransformFeedbacks", nullptr } },
                { (PRFuncPtr*) &mSymbols.fIsTransformFeedback, { "IsTransformFeedback", nullptr } },
                { (PRFuncPtr*) &mSymbols.fBeginTransformFeedback, { "BeginTransformFeedback", nullptr } },
                { (PRFuncPtr*) &mSymbols.fEndTransformFeedback, { "EndTransformFeedback", nullptr } },
                { (PRFuncPtr*) &mSymbols.fTransformFeedbackVaryings, { "TransformFeedbackVaryings", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetTransformFeedbackVarying, { "GetTransformFeedbackVarying", nullptr } },
                { (PRFuncPtr*) &mSymbols.fPauseTransformFeedback, { "PauseTransformFeedback", nullptr } },
                { (PRFuncPtr*) &mSymbols.fResumeTransformFeedback, { "ResumeTransformFeedback", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fBindBufferBase, { "BindBufferBaseEXT", "BindBufferBaseNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fBindBufferRange, { "BindBufferRangeEXT", "BindBufferRangeNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGenTransformFeedbacks, { "GenTransformFeedbacksNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDeleteTransformFeedbacks, { "DeleteTransformFeedbacksNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fIsTransformFeedback, { "IsTransformFeedbackNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fBeginTransformFeedback, { "BeginTransformFeedbackEXT", "BeginTransformFeedbackNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fEndTransformFeedback, { "EndTransformFeedbackEXT", "EndTransformFeedbackNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fTransformFeedbackVaryings, { "TransformFeedbackVaryingsEXT", "TransformFeedbackVaryingsNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetTransformFeedbackVarying, { "GetTransformFeedbackVaryingEXT", "GetTransformFeedbackVaryingNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fPauseTransformFeedback, { "PauseTransformFeedbackNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fResumeTransformFeedback, { "ResumeTransformFeedbackNV", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::transform_feedback2);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports transform feedback without supplying its functions.");

                MarkUnsupported(GLFeature::transform_feedback2);
                MarkUnsupported(GLFeature::bind_buffer_offset);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::bind_buffer_offset)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fBindBufferOffset, { "BindBufferOffset", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fBindBufferOffset,
                  { "BindBufferOffsetEXT", "BindBufferOffsetNV", nullptr }
                },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::bind_buffer_offset);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports BindBufferOffset without supplying its function.");

                MarkUnsupported(GLFeature::bind_buffer_offset);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::query_objects)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fBeginQuery, { "BeginQuery", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGenQueries, { "GenQueries", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDeleteQueries, { "DeleteQueries", nullptr } },
                { (PRFuncPtr*) &mSymbols.fEndQuery, { "EndQuery", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetQueryiv, { "GetQueryiv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetQueryObjectuiv, { "GetQueryObjectuiv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fIsQuery, { "IsQuery", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fBeginQuery, { "BeginQueryEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGenQueries, { "GenQueriesEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fDeleteQueries, { "DeleteQueriesEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fEndQuery, { "EndQueryEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetQueryiv, { "GetQueryivEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetQueryObjectuiv, { "GetQueryObjectuivEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fIsQuery, { "IsQueryEXT", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::query_objects);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports query objects without supplying its functions.");

                MarkUnsupported(GLFeature::query_objects);
                MarkUnsupported(GLFeature::get_query_object_iv);
                MarkUnsupported(GLFeature::occlusion_query);
                MarkUnsupported(GLFeature::occlusion_query_boolean);
                MarkUnsupported(GLFeature::occlusion_query2);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::get_query_object_iv)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetQueryObjectiv, { "GetQueryObjectiv", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetQueryObjectiv, { "GetQueryObjectivEXT", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::get_query_object_iv);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports query objects iv getter without supplying its function.");

                MarkUnsupported(GLFeature::get_query_object_iv);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::clear_buffers)) {
            SymLoadStruct clearBuffersSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fClearBufferfi,  { "ClearBufferfi",  nullptr } },
                { (PRFuncPtr*) &mSymbols.fClearBufferfv,  { "ClearBufferfv",  nullptr } },
                { (PRFuncPtr*) &mSymbols.fClearBufferiv,  { "ClearBufferiv",  nullptr } },
                { (PRFuncPtr*) &mSymbols.fClearBufferuiv, { "ClearBufferuiv", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(clearBuffersSymbols, trygl, prefix)) {
                NS_ERROR("GL supports clear_buffers without supplying its functions.");

                MarkUnsupported(GLFeature::clear_buffers);
                ClearSymbols(clearBuffersSymbols);
            }
        }

        if (IsSupported(GLFeature::copy_buffer)) {
            SymLoadStruct copyBufferSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fCopyBufferSubData, { "CopyBufferSubData", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(copyBufferSymbols, trygl, prefix)) {
                NS_ERROR("GL supports copy_buffer without supplying its function.");

                MarkUnsupported(GLFeature::copy_buffer);
                ClearSymbols(copyBufferSymbols);
            }
        }

        if (IsSupported(GLFeature::draw_buffers)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fDrawBuffers, { "DrawBuffers", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fDrawBuffers, { "DrawBuffersARB", "DrawBuffersEXT", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::draw_buffers);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports draw_buffers without supplying its functions.");

                MarkUnsupported(GLFeature::draw_buffers);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::draw_range_elements)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fDrawRangeElements, { "DrawRangeElements", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fDrawRangeElements, { "DrawRangeElementsEXT", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::draw_range_elements);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports draw_range_elements without supplying its functions.");

                MarkUnsupported(GLFeature::draw_range_elements);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::get_integer_indexed)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetIntegeri_v, { "GetIntegeri_v", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] ={
                { (PRFuncPtr*) &mSymbols.fGetIntegeri_v, { "GetIntegerIndexedvEXT", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::get_integer_indexed);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports get_integer_indexed without supplying its functions.");

                MarkUnsupported(GLFeature::get_integer_indexed);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::get_integer64_indexed)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetInteger64i_v, { "GetInteger64i_v", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(coreSymbols, trygl, prefix)) {
                NS_ERROR("GL supports get_integer64_indexed without supplying its functions.");

                MarkUnsupported(GLFeature::get_integer64_indexed);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::gpu_shader4)) {
            SymLoadStruct gpuShader4Symbols[] = {
                { (PRFuncPtr*) &mSymbols.fVertexAttribI4i, { "VertexAttribI4i", "VertexAttribI4iEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fVertexAttribI4iv, { "VertexAttribI4iv","VertexAttribI4ivEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fVertexAttribI4ui, { "VertexAttribI4ui", "VertexAttribI4uiEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fVertexAttribI4uiv, { "VertexAttribI4uiv", "VertexAttribI4uivEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fVertexAttribIPointer, { "VertexAttribIPointer", "VertexAttribIPointerEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniform1ui,  { "Uniform1ui", "Uniform1uiEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniform2ui,  { "Uniform2ui", "Uniform2uiEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniform3ui,  { "Uniform3ui", "Uniform3uiEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniform4ui,  { "Uniform4ui", "Uniform4uiEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniform1uiv, { "Uniform1uiv", "Uniform1uivEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniform2uiv, { "Uniform2uiv", "Uniform2uivEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniform3uiv, { "Uniform3uiv", "Uniform3uivEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniform4uiv, { "Uniform4uiv", "Uniform4uivEXT", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetFragDataLocation, { "GetFragDataLocation", "GetFragDataLocationEXT", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(gpuShader4Symbols, trygl, prefix)) {
                NS_ERROR("GL supports gpu_shader4 without supplying its functions.");

                MarkUnsupported(GLFeature::gpu_shader4);
                ClearSymbols(gpuShader4Symbols);
            }
        }

        if (IsSupported(GLFeature::map_buffer_range)) {
            SymLoadStruct mapBufferRangeSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fMapBufferRange, { "MapBufferRange", nullptr } },
                { (PRFuncPtr*) &mSymbols.fFlushMappedBufferRange, { "FlushMappedBufferRange", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(mapBufferRangeSymbols, trygl, prefix)) {
                NS_ERROR("GL supports map_buffer_range without supplying its functions.");

                MarkUnsupported(GLFeature::map_buffer_range);
                ClearSymbols(mapBufferRangeSymbols);
            }
        }

        if (IsSupported(GLFeature::texture_3D)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fTexImage3D, { "TexImage3D", nullptr } },
                { (PRFuncPtr*) &mSymbols.fTexSubImage3D, { "TexSubImage3D", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fTexSubImage3D, { "TexSubImage3DEXT", "TexSubImage3DOES", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::texture_3D);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports 3D textures without supplying functions.");

                MarkUnsupported(GLFeature::texture_3D);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::texture_3D_compressed)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fCompressedTexImage3D, { "CompressedTexImage3D", nullptr } },
                { (PRFuncPtr*) &mSymbols.fCompressedTexSubImage3D, { "CompressedTexSubImage3D", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fCompressedTexImage3D, { "CompressedTexImage3DARB", "CompressedTexImage3DOES", nullptr } },
                { (PRFuncPtr*) &mSymbols.fCompressedTexSubImage3D, { "CompressedTexSubImage3DARB", "CompressedTexSubImage3DOES", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::texture_3D_compressed);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports 3D textures without supplying functions.");

                MarkUnsupported(GLFeature::texture_3D_compressed);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::texture_3D_copy)) {
            SymLoadStruct coreSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fCopyTexSubImage3D, { "CopyTexSubImage3D", nullptr } },
                END_SYMBOLS
            };

            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fCopyTexSubImage3D, { "CopyTexSubImage3DEXT", "CopyTexSubImage3DOES", nullptr } },
                END_SYMBOLS
            };

            bool useCore = IsFeatureProvidedByCoreSymbols(GLFeature::texture_3D_copy);

            if (!LoadSymbols(useCore ? coreSymbols : extSymbols, trygl, prefix)) {
                NS_ERROR("GL supports 3D textures without supplying functions.");

                MarkUnsupported(GLFeature::texture_3D_copy);
                ClearSymbols(coreSymbols);
            }
        }

        if (IsSupported(GLFeature::uniform_buffer_object)) {
            SymLoadStruct uboSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetUniformIndices, { "GetUniformIndices", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetActiveUniformsiv, { "GetActiveUniformsiv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetActiveUniformName, { "GetActiveUniformName", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetUniformBlockIndex, { "GetUniformBlockIndex", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetActiveUniformBlockiv, { "GetActiveUniformBlockiv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetActiveUniformBlockName, { "GetActiveUniformBlockName", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniformBlockBinding, { "UniformBlockBinding", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(&uboSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports ARB_uniform_buffer_object without supplying its functions.");

                MarkExtensionUnsupported(ARB_uniform_buffer_object);
                MarkUnsupported(GLFeature::uniform_buffer_object);
                ClearSymbols(uboSymbols);
            }
        }

        if (IsSupported(GLFeature::uniform_matrix_nonsquare)) {
            SymLoadStruct umnSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fUniformMatrix2x3fv, { "UniformMatrix2x3fv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniformMatrix2x4fv, { "UniformMatrix2x4fv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniformMatrix3x2fv, { "UniformMatrix3x2fv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniformMatrix3x4fv, { "UniformMatrix3x4fv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniformMatrix4x2fv, { "UniformMatrix4x2fv", nullptr } },
                { (PRFuncPtr*) &mSymbols.fUniformMatrix4x3fv, { "UniformMatrix4x3fv", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(&umnSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports uniform matrix with non-square dim without supplying its functions.");

                MarkUnsupported(GLFeature::uniform_matrix_nonsquare);
                ClearSymbols(umnSymbols);
            }
        }

        if (IsSupported(GLFeature::invalidate_framebuffer)) {
            SymLoadStruct invSymbols[] = {
                { (PRFuncPtr *) &mSymbols.fInvalidateFramebuffer,    { "InvalidateFramebuffer", nullptr } },
                { (PRFuncPtr *) &mSymbols.fInvalidateSubFramebuffer, { "InvalidateSubFramebuffer", nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(&invSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports framebuffer invalidation without supplying its functions.");

                MarkUnsupported(GLFeature::invalidate_framebuffer);
                ClearSymbols(invSymbols);
            }
        }

        if (IsExtensionSupported(KHR_debug)) {
            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fDebugMessageControl,  { "DebugMessageControl",  "DebugMessageControlKHR",  nullptr } },
                { (PRFuncPtr*) &mSymbols.fDebugMessageInsert,   { "DebugMessageInsert",   "DebugMessageInsertKHR",   nullptr } },
                { (PRFuncPtr*) &mSymbols.fDebugMessageCallback, { "DebugMessageCallback", "DebugMessageCallbackKHR", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetDebugMessageLog,   { "GetDebugMessageLog",   "GetDebugMessageLogKHR",   nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetPointerv,          { "GetPointerv",          "GetPointervKHR",          nullptr } },
                { (PRFuncPtr*) &mSymbols.fPushDebugGroup,       { "PushDebugGroup",       "PushDebugGroupKHR",       nullptr } },
                { (PRFuncPtr*) &mSymbols.fPopDebugGroup,        { "PopDebugGroup",        "PopDebugGroupKHR",        nullptr } },
                { (PRFuncPtr*) &mSymbols.fObjectLabel,          { "ObjectLabel",          "ObjectLabelKHR",          nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetObjectLabel,       { "GetObjectLabel",       "GetObjectLabelKHR",       nullptr } },
                { (PRFuncPtr*) &mSymbols.fObjectPtrLabel,       { "ObjectPtrLabel",       "ObjectPtrLabelKHR",       nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetObjectPtrLabel,    { "GetObjectPtrLabel",    "GetObjectPtrLabelKHR",    nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(&extSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports KHR_debug without supplying its functions.");

                MarkExtensionUnsupported(KHR_debug);
                ClearSymbols(extSymbols);
            }
        }

        if (IsExtensionSupported(NV_fence)) {
            SymLoadStruct extSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGenFences,    { "GenFencesNV",    nullptr } },
                { (PRFuncPtr*) &mSymbols.fDeleteFences, { "DeleteFencesNV", nullptr } },
                { (PRFuncPtr*) &mSymbols.fSetFence,     { "SetFenceNV",     nullptr } },
                { (PRFuncPtr*) &mSymbols.fTestFence,    { "TestFenceNV",    nullptr } },
                { (PRFuncPtr*) &mSymbols.fFinishFence,  { "FinishFenceNV",  nullptr } },
                { (PRFuncPtr*) &mSymbols.fIsFence,      { "IsFenceNV",      nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetFenceiv,   { "GetFenceivNV",   nullptr } },
                END_SYMBOLS
            };

            if (!LoadSymbols(&extSymbols[0], trygl, prefix)) {
                NS_ERROR("GL supports NV_fence without supplying its functions.");

                MarkExtensionUnsupported(NV_fence);
                ClearSymbols(extSymbols);
            }
        }

        // Load developer symbols, don't fail if we can't find them.
        SymLoadStruct auxSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetTexImage, { "GetTexImage", nullptr } },
                { (PRFuncPtr*) &mSymbols.fGetTexLevelParameteriv, { "GetTexLevelParameteriv", nullptr } },
                END_SYMBOLS
        };
        bool warnOnFailures = DebugMode();
        LoadSymbols(&auxSymbols[0], trygl, prefix, warnOnFailures);
    }

    if (mInitialized) {
        raw_fGetIntegerv(LOCAL_GL_VIEWPORT, mViewportRect);
        raw_fGetIntegerv(LOCAL_GL_SCISSOR_BOX, mScissorRect);
        raw_fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
        raw_fGetIntegerv(LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE, &mMaxCubeMapTextureSize);
        raw_fGetIntegerv(LOCAL_GL_MAX_RENDERBUFFER_SIZE, &mMaxRenderbufferSize);
        raw_fGetIntegerv(LOCAL_GL_MAX_VIEWPORT_DIMS, mMaxViewportDims);

#ifdef XP_MACOSX
        if (mWorkAroundDriverBugs) {
            if (mVendor == GLVendor::Intel) {
                // see bug 737182 for 2D textures, bug 684882 for cube map textures.
                mMaxTextureSize        = std::min(mMaxTextureSize,        4096);
                mMaxCubeMapTextureSize = std::min(mMaxCubeMapTextureSize, 512);
                // for good measure, we align renderbuffers on what we do for 2D textures
                mMaxRenderbufferSize   = std::min(mMaxRenderbufferSize,   4096);
                mNeedsTextureSizeChecks = true;
            } else if (mVendor == GLVendor::NVIDIA) {
                if (nsCocoaFeatures::OnMountainLionOrLater()) {
                    // See bug 879656.  8192 fails, 8191 works.
                    mMaxTextureSize = std::min(mMaxTextureSize, 8191);
                    mMaxRenderbufferSize = std::min(mMaxRenderbufferSize, 8191);
                } else {
                    // See bug 877949.
                    mMaxTextureSize = std::min(mMaxTextureSize, 4096);
                    mMaxRenderbufferSize = std::min(mMaxRenderbufferSize, 4096);
                }

                // Part of the bug 879656, but it also doesn't hurt the 877949
                mNeedsTextureSizeChecks = true;
            }
        }
#endif
#ifdef MOZ_X11
        if (mWorkAroundDriverBugs &&
            mVendor == GLVendor::Nouveau) {
            // see bug 814716. Clamp MaxCubeMapTextureSize at 2K for Nouveau.
            mMaxCubeMapTextureSize = std::min(mMaxCubeMapTextureSize, 2048);
            mNeedsTextureSizeChecks = true;
        }
#endif

        mMaxTextureImageSize = mMaxTextureSize;

        mMaxSamples = 0;
        if (IsSupported(GLFeature::framebuffer_multisample)) {
            fGetIntegerv(LOCAL_GL_MAX_SAMPLES, (GLint*)&mMaxSamples);
        }

        // We're ready for final setup.
        fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

        if (mCaps.any)
            DetermineCaps();

        UpdatePixelFormat();
        UpdateGLFormats(mCaps);

        mTexGarbageBin = new TextureGarbageBin(this);

        MOZ_ASSERT(IsCurrent());

        if (DebugMode() && IsExtensionSupported(KHR_debug)) {
            fEnable(LOCAL_GL_DEBUG_OUTPUT);
            fDisable(LOCAL_GL_DEBUG_OUTPUT_SYNCHRONOUS);
            fDebugMessageCallback(&StaticDebugCallback, (void*)this);
            fDebugMessageControl(LOCAL_GL_DONT_CARE,
                                 LOCAL_GL_DONT_CARE,
                                 LOCAL_GL_DONT_CARE,
                                 0, nullptr,
                                 true);
        }

        reporter.SetSuccessful();
    } else {
        // if initialization fails, ensure all symbols are zero, to avoid hard-to-understand bugs
        mSymbols.Zero();
        NS_WARNING("InitWithPrefix failed!");
    }

    mVersionString = nsPrintfCString("%u.%u.%u", mVersion / 100, (mVersion / 10) % 10, mVersion % 10);

    return mInitialized;
}

void
GLContext::DebugCallback(GLenum source,
                         GLenum type,
                         GLuint id,
                         GLenum severity,
                         GLsizei length,
                         const GLchar* message)
{
    nsAutoCString sourceStr;
    switch (source) {
    case LOCAL_GL_DEBUG_SOURCE_API:
        sourceStr = NS_LITERAL_CSTRING("SOURCE_API");
        break;
    case LOCAL_GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        sourceStr = NS_LITERAL_CSTRING("SOURCE_WINDOW_SYSTEM");
        break;
    case LOCAL_GL_DEBUG_SOURCE_SHADER_COMPILER:
        sourceStr = NS_LITERAL_CSTRING("SOURCE_SHADER_COMPILER");
        break;
    case LOCAL_GL_DEBUG_SOURCE_THIRD_PARTY:
        sourceStr = NS_LITERAL_CSTRING("SOURCE_THIRD_PARTY");
        break;
    case LOCAL_GL_DEBUG_SOURCE_APPLICATION:
        sourceStr = NS_LITERAL_CSTRING("SOURCE_APPLICATION");
        break;
    case LOCAL_GL_DEBUG_SOURCE_OTHER:
        sourceStr = NS_LITERAL_CSTRING("SOURCE_OTHER");
        break;
    default:
        sourceStr = nsPrintfCString("<source 0x%04x>", source);
        break;
    }

    nsAutoCString typeStr;
    switch (type) {
    case LOCAL_GL_DEBUG_TYPE_ERROR:
        typeStr = NS_LITERAL_CSTRING("TYPE_ERROR");
        break;
    case LOCAL_GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        typeStr = NS_LITERAL_CSTRING("TYPE_DEPRECATED_BEHAVIOR");
        break;
    case LOCAL_GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        typeStr = NS_LITERAL_CSTRING("TYPE_UNDEFINED_BEHAVIOR");
        break;
    case LOCAL_GL_DEBUG_TYPE_PORTABILITY:
        typeStr = NS_LITERAL_CSTRING("TYPE_PORTABILITY");
        break;
    case LOCAL_GL_DEBUG_TYPE_PERFORMANCE:
        typeStr = NS_LITERAL_CSTRING("TYPE_PERFORMANCE");
        break;
    case LOCAL_GL_DEBUG_TYPE_OTHER:
        typeStr = NS_LITERAL_CSTRING("TYPE_OTHER");
        break;
    case LOCAL_GL_DEBUG_TYPE_MARKER:
        typeStr = NS_LITERAL_CSTRING("TYPE_MARKER");
        break;
    default:
        typeStr = nsPrintfCString("<type 0x%04x>", type);
        break;
    }

    nsAutoCString sevStr;
    switch (severity) {
    case LOCAL_GL_DEBUG_SEVERITY_HIGH:
        sevStr = NS_LITERAL_CSTRING("SEVERITY_HIGH");
        break;
    case LOCAL_GL_DEBUG_SEVERITY_MEDIUM:
        sevStr = NS_LITERAL_CSTRING("SEVERITY_MEDIUM");
        break;
    case LOCAL_GL_DEBUG_SEVERITY_LOW:
        sevStr = NS_LITERAL_CSTRING("SEVERITY_LOW");
        break;
    case LOCAL_GL_DEBUG_SEVERITY_NOTIFICATION:
        sevStr = NS_LITERAL_CSTRING("SEVERITY_NOTIFICATION");
        break;
    default:
        sevStr = nsPrintfCString("<severity 0x%04x>", severity);
        break;
    }

    printf_stderr("[KHR_debug: 0x%" PRIxPTR "] ID %u: %s %s %s:\n    %s",
                  (uintptr_t)this,
                  id,
                  sourceStr.BeginReading(),
                  typeStr.BeginReading(),
                  sevStr.BeginReading(),
                  message);
}

void
GLContext::InitExtensions()
{
    MakeCurrent();
    const char* extensions = (const char*)fGetString(LOCAL_GL_EXTENSIONS);
    if (!extensions)
        return;

#ifdef MOZ_GL_DEBUG
    static bool firstRun = true;
#else
    // Non-DEBUG, so never spew.
    const bool firstRun = false;
#endif

    InitializeExtensionsBitSet(mAvailableExtensions, extensions, sExtensionNames, firstRun && DebugMode());

    if (WorkAroundDriverBugs() &&
        Vendor() == GLVendor::Qualcomm) {

        // Some Adreno drivers do not report GL_OES_EGL_sync, but they really do support it.
        MarkExtensionSupported(OES_EGL_sync);
    }

    if (WorkAroundDriverBugs() &&
        Renderer() == GLRenderer::AndroidEmulator) {
        // the Android emulator, which we use to run B2G reftests on,
        // doesn't expose the OES_rgb8_rgba8 extension, but it seems to
        // support it (tautologically, as it only runs on desktop GL).
        MarkExtensionSupported(OES_rgb8_rgba8);
    }

    if (WorkAroundDriverBugs() &&
        Vendor() == GLVendor::VMware &&
        Renderer() == GLRenderer::GalliumLlvmpipe)
    {
        // The llvmpipe driver that is used on linux try servers appears to have
        // buggy support for s3tc/dxt1 compressed textures.
        // See Bug 975824.
        MarkExtensionUnsupported(EXT_texture_compression_s3tc);
        MarkExtensionUnsupported(EXT_texture_compression_dxt1);
        MarkExtensionUnsupported(ANGLE_texture_compression_dxt3);
        MarkExtensionUnsupported(ANGLE_texture_compression_dxt5);
    }

#ifdef XP_MACOSX
    // Bug 1009642: On OSX Mavericks (10.9), the driver for Intel HD
    // 3000 appears to be buggy WRT updating sub-images of S3TC
    // textures with glCompressedTexSubImage2D. Works on Intel HD 4000
    // and Intel HD 5000/Iris that I tested.
    if (WorkAroundDriverBugs() &&
        nsCocoaFeatures::OSXVersionMajor() == 10 &&
        nsCocoaFeatures::OSXVersionMinor() == 9 &&
        Renderer() == GLRenderer::IntelHD3000)
    {
        MarkExtensionUnsupported(EXT_texture_compression_s3tc);
    }
#endif

#ifdef MOZ_GL_DEBUG
    firstRun = false;
#endif
}

void
GLContext::PlatformStartup()
{
  RegisterStrongMemoryReporter(new GfxTexturesReporter());
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

void
GLContext::DetermineCaps()
{
    PixelBufferFormat format = QueryPixelFormat();

    SurfaceCaps caps;
    caps.color = !!format.red && !!format.green && !!format.blue;
    caps.bpp16 = caps.color && format.ColorBits() == 16;
    caps.alpha = !!format.alpha;
    caps.depth = !!format.depth;
    caps.stencil = !!format.stencil;
    caps.antialias = format.samples > 1;
    caps.preserve = true;

    mCaps = caps;
}

PixelBufferFormat
GLContext::QueryPixelFormat()
{
    PixelBufferFormat format;

    ScopedBindFramebuffer autoFB(this, 0);

    fGetIntegerv(LOCAL_GL_RED_BITS  , &format.red  );
    fGetIntegerv(LOCAL_GL_GREEN_BITS, &format.green);
    fGetIntegerv(LOCAL_GL_BLUE_BITS , &format.blue );
    fGetIntegerv(LOCAL_GL_ALPHA_BITS, &format.alpha);

    fGetIntegerv(LOCAL_GL_DEPTH_BITS, &format.depth);
    fGetIntegerv(LOCAL_GL_STENCIL_BITS, &format.stencil);

    fGetIntegerv(LOCAL_GL_SAMPLES, &format.samples);

    return format;
}

void
GLContext::UpdatePixelFormat()
{
    PixelBufferFormat format = QueryPixelFormat();
#ifdef MOZ_GL_DEBUG
    const SurfaceCaps& caps = Caps();
    MOZ_ASSERT(!caps.any, "Did you forget to DetermineCaps()?");

    MOZ_ASSERT(caps.color == !!format.red);
    MOZ_ASSERT(caps.color == !!format.green);
    MOZ_ASSERT(caps.color == !!format.blue);

    // These we either must have if they're requested, or
    // we can have if they're not.
    MOZ_ASSERT(caps.alpha == !!format.alpha || !caps.alpha);
    MOZ_ASSERT(caps.depth == !!format.depth || !caps.depth);
    MOZ_ASSERT(caps.stencil == !!format.stencil || !caps.stencil);

    MOZ_ASSERT(caps.antialias == (format.samples > 1));
#endif
    mPixelFormat = new PixelBufferFormat(format);
}

GLFormats
GLContext::ChooseGLFormats(const SurfaceCaps& caps) const
{
    GLFormats formats;

    // If we're on ES2 hardware and we have an explicit request for 16 bits of color or less
    // OR we don't support full 8-bit color, return a 4444 or 565 format.
    bool bpp16 = caps.bpp16;
    if (IsGLES()) {
        if (!IsExtensionSupported(OES_rgb8_rgba8))
            bpp16 = true;
    } else {
        // RGB565 is uncommon on desktop, requiring ARB_ES2_compatibility.
        // Since it's also vanishingly useless there, let's not support it.
        bpp16 = false;
    }

    if (bpp16) {
        MOZ_ASSERT(IsGLES());
        if (caps.alpha) {
            formats.color_texInternalFormat = LOCAL_GL_RGBA;
            formats.color_texFormat = LOCAL_GL_RGBA;
            formats.color_texType   = LOCAL_GL_UNSIGNED_SHORT_4_4_4_4;
            formats.color_rbFormat  = LOCAL_GL_RGBA4;
        } else {
            formats.color_texInternalFormat = LOCAL_GL_RGB;
            formats.color_texFormat = LOCAL_GL_RGB;
            formats.color_texType   = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
            formats.color_rbFormat  = LOCAL_GL_RGB565;
        }
    } else {
        formats.color_texType = LOCAL_GL_UNSIGNED_BYTE;

        if (caps.alpha) {
            formats.color_texInternalFormat = IsGLES() ? LOCAL_GL_RGBA : LOCAL_GL_RGBA8;
            formats.color_texFormat = LOCAL_GL_RGBA;
            formats.color_rbFormat  = LOCAL_GL_RGBA8;
        } else {
            formats.color_texInternalFormat = IsGLES() ? LOCAL_GL_RGB : LOCAL_GL_RGB8;
            formats.color_texFormat = LOCAL_GL_RGB;
            formats.color_rbFormat  = LOCAL_GL_RGB8;
        }
    }

    uint32_t msaaLevel = gfxPrefs::MSAALevel();
    GLsizei samples = msaaLevel * msaaLevel;
    samples = std::min(samples, mMaxSamples);

    // Bug 778765.
    if (WorkAroundDriverBugs() && samples == 1) {
        samples = 0;
    }
    formats.samples = samples;


    // Be clear that these are 0 if unavailable.
    formats.depthStencil = 0;
    if (!IsGLES() || IsExtensionSupported(OES_packed_depth_stencil)) {
        formats.depthStencil = LOCAL_GL_DEPTH24_STENCIL8;
    }

    formats.depth = 0;
    if (IsGLES()) {
        if (IsExtensionSupported(OES_depth24)) {
            formats.depth = LOCAL_GL_DEPTH_COMPONENT24;
        } else {
            formats.depth = LOCAL_GL_DEPTH_COMPONENT16;
        }
    } else {
        formats.depth = LOCAL_GL_DEPTH_COMPONENT24;
    }

    formats.stencil = LOCAL_GL_STENCIL_INDEX8;

    return formats;
}

bool
GLContext::IsFramebufferComplete(GLuint fb, GLenum* pStatus)
{
    MOZ_ASSERT(fb);

    ScopedBindFramebuffer autoFB(this, fb);
    MOZ_ASSERT(fIsFramebuffer(fb));

    GLenum status = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (pStatus)
        *pStatus = status;

    return status == LOCAL_GL_FRAMEBUFFER_COMPLETE;
}

void
GLContext::AttachBuffersToFB(GLuint colorTex, GLuint colorRB,
                             GLuint depthRB, GLuint stencilRB,
                             GLuint fb, GLenum target)
{
    MOZ_ASSERT(fb);
    MOZ_ASSERT( !(colorTex && colorRB) );

    ScopedBindFramebuffer autoFB(this, fb);
    MOZ_ASSERT(fIsFramebuffer(fb)); // It only counts after being bound.

    if (colorTex) {
        MOZ_ASSERT(fIsTexture(colorTex));
        MOZ_ASSERT(target == LOCAL_GL_TEXTURE_2D ||
                   target == LOCAL_GL_TEXTURE_RECTANGLE_ARB);
        fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                              LOCAL_GL_COLOR_ATTACHMENT0,
                              target,
                              colorTex,
                              0);
    } else if (colorRB) {
        MOZ_ASSERT(fIsRenderbuffer(colorRB));
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_COLOR_ATTACHMENT0,
                                 LOCAL_GL_RENDERBUFFER,
                                 colorRB);
    }

    if (depthRB) {
        MOZ_ASSERT(fIsRenderbuffer(depthRB));
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER,
                                 depthRB);
    }

    if (stencilRB) {
        MOZ_ASSERT(fIsRenderbuffer(stencilRB));
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_STENCIL_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER,
                                 stencilRB);
    }
}

bool
GLContext::AssembleOffscreenFBs(const GLuint colorMSRB,
                                const GLuint depthRB,
                                const GLuint stencilRB,
                                const GLuint texture,
                                GLuint* drawFB_out,
                                GLuint* readFB_out)
{
    if (!colorMSRB && !texture) {
        MOZ_ASSERT(!depthRB && !stencilRB);

        if (drawFB_out)
            *drawFB_out = 0;
        if (readFB_out)
            *readFB_out = 0;

        return true;
    }

    ScopedBindFramebuffer autoFB(this);

    GLuint drawFB = 0;
    GLuint readFB = 0;

    if (texture) {
        readFB = 0;
        fGenFramebuffers(1, &readFB);
        BindFB(readFB);
        fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                              LOCAL_GL_COLOR_ATTACHMENT0,
                              LOCAL_GL_TEXTURE_2D,
                              texture,
                              0);
    }

    if (colorMSRB) {
        drawFB = 0;
        fGenFramebuffers(1, &drawFB);
        BindFB(drawFB);
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_COLOR_ATTACHMENT0,
                                 LOCAL_GL_RENDERBUFFER,
                                 colorMSRB);
    } else {
        drawFB = readFB;
    }
    MOZ_ASSERT(GetFB() == drawFB);

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

    if (!IsFramebufferComplete(drawFB, &status)) {
        NS_WARNING("DrawFBO: Incomplete");
  #ifdef MOZ_GL_DEBUG
        if (DebugMode()) {
            printf_stderr("Framebuffer status: %X\n", status);
        }
  #endif
        isComplete = false;
    }

    if (!IsFramebufferComplete(readFB, &status)) {
        NS_WARNING("ReadFBO: Incomplete");
  #ifdef MOZ_GL_DEBUG
        if (DebugMode()) {
            printf_stderr("Framebuffer status: %X\n", status);
        }
  #endif
        isComplete = false;
    }

    if (drawFB_out) {
        *drawFB_out = drawFB;
    } else if (drawFB) {
        NS_RUNTIMEABORT("drawFB created when not requested!");
    }

    if (readFB_out) {
        *readFB_out = readFB;
    } else if (readFB) {
        NS_RUNTIMEABORT("readFB created when not requested!");
    }

    return isComplete;
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
GLContext::MarkDestroyed()
{
    if (IsDestroyed())
        return;

    if (MakeCurrent()) {
        DestroyScreenBuffer();

        mBlitHelper = nullptr;
        mBlitTextureImageHelper = nullptr;
        mReadTexImageHelper = nullptr;

        mTexGarbageBin->GLContextTeardown();
    } else {
        NS_WARNING("MakeCurrent() failed during MarkDestroyed! Skipping GL object teardown.");
    }

    mSymbols.Zero();
}

#ifdef MOZ_GL_DEBUG
/* static */ void
GLContext::AssertNotPassingStackBufferToTheGL(const void* ptr)
{
  int somethingOnTheStack;
  const void* someStackPtr = &somethingOnTheStack;
  const int page_bits = 12;
  intptr_t page = reinterpret_cast<uintptr_t>(ptr) >> page_bits;
  intptr_t someStackPage = reinterpret_cast<uintptr_t>(someStackPtr) >> page_bits;
  uintptr_t pageDistance = std::abs(page - someStackPage);

  // Explanation for the "distance <= 1" check here as opposed to just
  // an equality check.
  //
  // Here we assume that pages immediately adjacent to the someStackAddress page,
  // are also stack pages. That allows to catch the case where the calling frame put
  // a buffer on the stack, and we just crossed the page boundary. That is likely
  // to happen, precisely, when using stack arrays. I hit that specifically
  // with CompositorOGL::Initialize.
  //
  // In theory we could be unlucky and wrongly assert here. If that happens,
  // it will only affect debug builds, and looking at stacks we'll be able to
  // see that this assert is wrong and revert to the conservative and safe
  // approach of only asserting when address and someStackAddress are
  // on the same page.
  bool isStackAddress = pageDistance <= 1;
  MOZ_ASSERT(!isStackAddress,
             "Please don't pass stack arrays to the GL. "
             "Consider using HeapCopyOfStackArray. "
             "See bug 1005658.");
}

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
GLContext::CreatedQueries(GLContext *aOrigin, GLsizei aCount, GLuint *aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedQueries.AppendElement(NamedResource(aOrigin, aNames[i]));
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
RemoveNamesFromArray(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames, nsTArray<GLContext::NamedResource>& aArray)
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
GLContext::DeletedBuffers(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedBuffers);
}

void
GLContext::DeletedQueries(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedQueries);
}

void
GLContext::DeletedTextures(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedTextures);
}

void
GLContext::DeletedFramebuffers(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedFramebuffers);
}

void
GLContext::DeletedRenderbuffers(GLContext *aOrigin, GLsizei aCount, const GLuint *aNames)
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
    MarkContextDestroyedInArray(aChild, mTrackedQueries);
}

static void
ReportArrayContents(const char *title, const nsTArray<GLContext::NamedResource>& aArray)
{
    if (aArray.Length() == 0)
        return;

    printf_stderr("%s:\n", title);

    nsTArray<GLContext::NamedResource> copy(aArray);
    copy.Sort();

    GLContext *lastContext = nullptr;
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
    ReportArrayContents("Outstanding Queries", mTrackedQueries);
    ReportArrayContents("Outstanding Programs", mTrackedPrograms);
    ReportArrayContents("Outstanding Shaders", mTrackedShaders);
    ReportArrayContents("Outstanding Framebuffers", mTrackedFramebuffers);
    ReportArrayContents("Outstanding Renderbuffers", mTrackedRenderbuffers);
}

#endif /* DEBUG */


void
GLContext::GuaranteeResolve()
{
    if (mScreen) {
        mScreen->AssureBlitted();
    }
    fFinish();
}

const gfx::IntSize&
GLContext::OffscreenSize() const
{
    MOZ_ASSERT(IsOffscreen());
    return mScreen->Size();
}

bool
GLContext::CreateScreenBufferImpl(const IntSize& size, const SurfaceCaps& caps)
{
    UniquePtr<GLScreenBuffer> newScreen = GLScreenBuffer::Create(this, size, caps);
    if (!newScreen)
        return false;

    if (!newScreen->Resize(size)) {
        return false;
    }

    DestroyScreenBuffer();

    // This will rebind to 0 (Screen) if needed when
    // it falls out of scope.
    ScopedBindFramebuffer autoFB(this);

    mScreen = Move(newScreen);

    return true;
}

bool
GLContext::ResizeScreenBuffer(const IntSize& size)
{
    if (!IsOffscreenSizeAllowed(size))
        return false;

    return mScreen->Resize(size);
}

void
GLContext::DestroyScreenBuffer()
{
    mScreen = nullptr;
}

void
GLContext::ForceDirtyScreen()
{
    ScopedBindFramebuffer autoFB(0);

    BeforeGLDrawCall();
    // no-op; just pretend we did something
    AfterGLDrawCall();
}

void
GLContext::CleanDirtyScreen()
{
    ScopedBindFramebuffer autoFB(0);

    BeforeGLReadCall();
    // no-op; we just want to make sure the Read FBO is updated if it needs to be
    AfterGLReadCall();
}

void
GLContext::EmptyTexGarbageBin()
{
   TexGarbageBin()->EmptyGarbage();
}

bool
GLContext::IsOffscreenSizeAllowed(const IntSize& aSize) const {
  int32_t biggerDimension = std::max(aSize.width, aSize.height);
  int32_t maxAllowed = std::min(mMaxRenderbufferSize, mMaxTextureSize);
  return biggerDimension <= maxAllowed;
}

bool
GLContext::IsOwningThreadCurrent()
{
  return PlatformThread::CurrentId() == mOwningThreadId;
}

GLBlitHelper*
GLContext::BlitHelper()
{
    if (!mBlitHelper) {
        mBlitHelper = MakeUnique<GLBlitHelper>(this);
    }

    return mBlitHelper.get();
}

GLBlitTextureImageHelper*
GLContext::BlitTextureImageHelper()
{
    if (!mBlitTextureImageHelper) {
        mBlitTextureImageHelper = MakeUnique<GLBlitTextureImageHelper>(this);
    }

    return mBlitTextureImageHelper.get();
}

GLReadTexImageHelper*
GLContext::ReadTexImageHelper()
{
    if (!mReadTexImageHelper) {
        mReadTexImageHelper = MakeUnique<GLReadTexImageHelper>(this);
    }

    return mReadTexImageHelper.get();
}

void
GLContext::FlushIfHeavyGLCallsSinceLastFlush()
{
    if (!mHeavyGLCallsSinceLastFlush) {
        return;
    }
    MakeCurrent();
    fFlush();
}

bool
DoesStringMatch(const char* aString, const char *aWantedString)
{
    if (!aString || !aWantedString)
        return false;

    const char *occurrence = strstr(aString, aWantedString);

    // aWanted not found
    if (!occurrence)
        return false;

    // aWantedString preceded by alpha character
    if (occurrence != aString && isalpha(*(occurrence-1)))
        return false;

    // aWantedVendor followed by alpha character
    const char *afterOccurrence = occurrence + strlen(aWantedString);
    if (isalpha(*afterOccurrence))
        return false;

    return true;
}

} /* namespace gl */
} /* namespace mozilla */
