/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <vector>
#ifdef MOZ_WIDGET_ANDROID
#include <fcntl.h>
#include <sys/mman.h>
#endif

#include "GLBlitHelper.h"
#include "GLReadTexImageHelper.h"
#include "GLScreenBuffer.h"

#include "gfxCrashReporterUtils.h"
#include "gfxEnv.h"
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
#include "mozilla/gfx/Logging.h"

#include "OGLShaderProgram.h" // for ShaderProgramType

#include "mozilla/DebugOnly.h"

#ifdef XP_MACOSX
#include <CoreServices/CoreServices.h>
#endif

#if defined(MOZ_WIDGET_COCOA)
#include "nsCocoaFeatures.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::layers;

#ifdef MOZ_GL_DEBUG
unsigned GLContext::sCurrentGLContextTLS = -1;
#endif

// If adding defines, don't forget to undefine symbols. See #undef block below.
#define CORE_SYMBOL(x) { (PRFuncPtr*) &mSymbols.f##x, { #x, nullptr } }
#define CORE_EXT_SYMBOL2(x,y,z) { (PRFuncPtr*) &mSymbols.f##x, { #x, #x #y, #x #z, nullptr } }
#define EXT_SYMBOL2(x,y,z) { (PRFuncPtr*) &mSymbols.f##x, { #x #y, #x #z, nullptr } }
#define EXT_SYMBOL3(x,y,z,w) { (PRFuncPtr*) &mSymbols.f##x, { #x #y, #x #z, #x #w, nullptr } }
#define END_SYMBOLS { nullptr, { nullptr } }

// should match the order of GLExtensions, and be null-terminated.
static const char* const sExtensionNames[] = {
    "NO_EXTENSION",
    "GL_AMD_compressed_ATC_texture",
    "GL_ANGLE_depth_texture",
    "GL_ANGLE_framebuffer_blit",
    "GL_ANGLE_framebuffer_multisample",
    "GL_ANGLE_instanced_arrays",
    "GL_ANGLE_texture_compression_dxt3",
    "GL_ANGLE_texture_compression_dxt5",
    "GL_ANGLE_timer_query",
    "GL_APPLE_client_storage",
    "GL_APPLE_framebuffer_multisample",
    "GL_APPLE_sync",
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
    "GL_ARB_geometry_shader4",
    "GL_ARB_half_float_pixel",
    "GL_ARB_instanced_arrays",
    "GL_ARB_internalformat_query",
    "GL_ARB_invalidate_subdata",
    "GL_ARB_map_buffer_range",
    "GL_ARB_occlusion_query2",
    "GL_ARB_pixel_buffer_object",
    "GL_ARB_robustness",
    "GL_ARB_sampler_objects",
    "GL_ARB_seamless_cube_map",
    "GL_ARB_shader_texture_lod",
    "GL_ARB_sync",
    "GL_ARB_texture_compression",
    "GL_ARB_texture_float",
    "GL_ARB_texture_non_power_of_two",
    "GL_ARB_texture_rectangle",
    "GL_ARB_texture_rg",
    "GL_ARB_texture_storage",
    "GL_ARB_texture_swizzle",
    "GL_ARB_timer_query",
    "GL_ARB_transform_feedback2",
    "GL_ARB_uniform_buffer_object",
    "GL_ARB_vertex_array_object",
    "GL_EXT_bgra",
    "GL_EXT_blend_minmax",
    "GL_EXT_color_buffer_float",
    "GL_EXT_color_buffer_half_float",
    "GL_EXT_copy_texture",
    "GL_EXT_disjoint_timer_query",
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
    "GL_EXT_multisampled_render_to_texture",
    "GL_EXT_occlusion_query_boolean",
    "GL_EXT_packed_depth_stencil",
    "GL_EXT_read_format_bgra",
    "GL_EXT_robustness",
    "GL_EXT_sRGB",
    "GL_EXT_sRGB_write_control",
    "GL_EXT_shader_texture_lod",
    "GL_EXT_texture3D",
    "GL_EXT_texture_compression_dxt1",
    "GL_EXT_texture_compression_s3tc",
    "GL_EXT_texture_filter_anisotropic",
    "GL_EXT_texture_format_BGRA8888",
    "GL_EXT_texture_sRGB",
    "GL_EXT_texture_storage",
    "GL_EXT_timer_query",
    "GL_EXT_transform_feedback",
    "GL_EXT_unpack_subimage",
    "GL_IMG_read_format",
    "GL_IMG_texture_compression_pvrtc",
    "GL_IMG_texture_npot",
    "GL_KHR_debug",
    "GL_NV_draw_instanced",
    "GL_NV_fence",
    "GL_NV_framebuffer_blit",
    "GL_NV_geometry_program4",
    "GL_NV_half_float",
    "GL_NV_instanced_arrays",
    "GL_NV_texture_barrier",
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
    "GL_OES_framebuffer_object",
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
    "GL_OES_vertex_array_object"
};

static bool
ParseGLSLVersion(GLContext* gl, uint32_t* out_version)
{
    if (gl->fGetError() != LOCAL_GL_NO_ERROR) {
        MOZ_ASSERT(false, "An OpenGL error has been triggered before.");
        return false;
    }

    /**
     * OpenGL 2.x, 3.x, 4.x specifications:
     *  The VERSION and SHADING_LANGUAGE_VERSION strings are laid out as follows:
     *
     *    <version number><space><vendor-specific information>
     *
     *  The version number is either of the form major_number.minor_number or
     *  major_number.minor_number.release_number, where the numbers all have
     *  one or more digits.
     *
     * SHADING_LANGUAGE_VERSION is *almost* identical to VERSION. The
     * difference is that the minor version always has two digits and the
     * prefix has an additional 'GLSL ES'
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
    const char* versionString = (const char*) gl->fGetString(LOCAL_GL_SHADING_LANGUAGE_VERSION);

    if (gl->fGetError() != LOCAL_GL_NO_ERROR) {
        MOZ_ASSERT(false, "glGetString(GL_SHADING_LANGUAGE_VERSION) has generated an error");
        return false;
    }

    if (!versionString) {
        // This happens on the Android emulators. We'll just return 100
        *out_version = 100;
        return true;
    }

    const auto fnSkipPrefix = [&versionString](const char* prefix) {
        const auto len = strlen(prefix);
        if (strncmp(versionString, prefix, len) == 0)
            versionString += len;
    };

    const char kGLESVersionPrefix[] = "OpenGL ES GLSL ES";
    fnSkipPrefix(kGLESVersionPrefix);

    if (gl->WorkAroundDriverBugs()) {
        // Nexus 7 2013 (bug 1234441)
        const char kBadGLESVersionPrefix[] = "OpenGL ES GLSL";
        fnSkipPrefix(kBadGLESVersionPrefix);
    }

    const char* itr = versionString;
    char* end = nullptr;
    auto majorVersion = strtol(itr, &end, 10);

    if (!end) {
        MOZ_ASSERT(false, "Failed to parse the GL major version number.");
        return false;
    }

    if (*end != '.') {
        MOZ_ASSERT(false, "Failed to parse GL's major-minor version number separator.");
        return false;
    }

    // we skip the '.' between the major and the minor version
    itr = end + 1;
    end = nullptr;

    auto minorVersion = strtol(itr, &end, 10);
    if (!end) {
        MOZ_ASSERT(false, "Failed to parse GL's minor version number.");
        return false;
    }

    if (majorVersion <= 0 || majorVersion >= 100) {
        MOZ_ASSERT(false, "Invalid major version.");
        return false;
    }

    if (minorVersion < 0 || minorVersion >= 100) {
        MOZ_ASSERT(false, "Invalid minor version.");
        return false;
    }

    *out_version = (uint32_t) majorVersion * 100 + (uint32_t) minorVersion;
    return true;
}

static bool
ParseGLVersion(GLContext* gl, uint32_t* out_version)
{
    if (gl->fGetError() != LOCAL_GL_NO_ERROR) {
        MOZ_ASSERT(false, "An OpenGL error has been triggered before.");
        return false;
    }

    /**
     * B2G emulator bug work around: The emulator implements OpenGL ES 2.0 on
     * OpenGL 3.2. The bug is that GetIntegerv(LOCAL_GL_{MAJOR,MINOR}_VERSION)
     * returns OpenGL 3.2 instead of generating an error.
     */
    if (!gl->IsGLES()) {
        /**
         * OpenGL 3.1 and OpenGL ES 3.0 both introduce GL_{MAJOR,MINOR}_VERSION
         * with GetIntegerv. So we first try those constants even though we
         * might not have an OpenGL context supporting them, as this is a
         * better way than parsing GL_VERSION.
         */
        GLint majorVersion = 0;
        GLint minorVersion = 0;

        const bool ok = (gl->GetPotentialInteger(LOCAL_GL_MAJOR_VERSION,
                                                 &majorVersion) &&
                         gl->GetPotentialInteger(LOCAL_GL_MINOR_VERSION,
                                                 &minorVersion));

        // If it's not an OpenGL (ES) 3.0 context, we will have an error
        if (ok &&
            majorVersion > 0 &&
            minorVersion >= 0)
        {
            *out_version = majorVersion * 100 + minorVersion * 10;
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

    if (gl->fGetError() != LOCAL_GL_NO_ERROR) {
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
    auto majorVersion = strtol(itr, &end, 10);

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

    auto minorVersion = strtol(itr, &end, 10);
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

    *out_version = (uint32_t)majorVersion * 100 + (uint32_t)minorVersion * 10;
    return true;
}

static uint8_t
ChooseDebugFlags(CreateContextFlags createFlags)
{
    uint8_t debugFlags = 0;

#ifdef MOZ_GL_DEBUG
    if (gfxEnv::GlDebug()) {
        debugFlags |= GLContext::DebugFlagEnabled;
    }

    // Enables extra verbose output, informing of the start and finish of every GL call.
    // Useful e.g. to record information to investigate graphics system crashes/lockups
    if (gfxEnv::GlDebugVerbose()) {
        debugFlags |= GLContext::DebugFlagTrace;
    }

    // Aborts on GL error. Can be useful to debug quicker code that is known not to
    // generate any GL error in principle.
    bool abortOnError = false;

    if (createFlags & CreateContextFlags::NO_VALIDATION) {
        abortOnError = true;

        const auto fnStringsMatch = [](const char* a, const char* b) {
            return strcmp(a, b) == 0;
        };

        const char* envAbortOnError = PR_GetEnv("MOZ_GL_DEBUG_ABORT_ON_ERROR");
        if (envAbortOnError && fnStringsMatch(envAbortOnError, "0")) {
           abortOnError = false;
        }
    }

    if (abortOnError) {
        debugFlags |= GLContext::DebugFlagAbortOnError;
    }
#endif

    return debugFlags;
}

GLContext::GLContext(CreateContextFlags flags, const SurfaceCaps& caps,
                     GLContext* sharedContext, bool isOffscreen)
  : mIsOffscreen(isOffscreen),
    mContextLost(false),
    mVersion(0),
    mProfile(ContextProfile::Unknown),
    mShadingLanguageVersion(0),
    mVendor(GLVendor::Other),
    mRenderer(GLRenderer::Other),
    mTopError(LOCAL_GL_NO_ERROR),
    mDebugFlags(ChooseDebugFlags(flags)),
    mSharedContext(sharedContext),
    mCaps(caps),
    mScreen(nullptr),
    mLockedSurface(nullptr),
    mMaxTextureSize(0),
    mMaxCubeMapTextureSize(0),
    mMaxTextureImageSize(0),
    mMaxRenderbufferSize(0),
    mMaxSamples(0),
    mNeedsTextureSizeChecks(false),
    mNeedsFlushBeforeDeleteFB(false),
    mTextureAllocCrashesOnMapFailure(false),
    mWorkAroundDriverBugs(true),
    mHeavyGLCallsSinceLastFlush(false)
{
    mMaxViewportDims[0] = 0;
    mMaxViewportDims[1] = 0;
    mOwningThreadId = PlatformThread::CurrentId();
}

GLContext::~GLContext() {
    NS_ASSERTION(IsDestroyed(), "GLContext implementation must call MarkDestroyed in destructor!");
#ifdef MOZ_GL_DEBUG
    if (mSharedContext) {
        GLContext* tip = mSharedContext;
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
ClearSymbols(const GLLibraryLoader::SymLoadStruct* symbols)
{
    while (symbols->symPointer) {
        *symbols->symPointer = nullptr;
        symbols++;
    }
}

bool
GLContext::InitWithPrefix(const char* prefix, bool trygl)
{
    MOZ_RELEASE_ASSERT(!mSymbols.fBindFramebuffer,
                       "GFX: InitWithPrefix should only be called once.");

    ScopedGfxFeatureReporter reporter("GL Context");

    if (!InitWithPrefixImpl(prefix, trygl)) {
        // If initialization fails, zero the symbols to avoid hard-to-understand bugs.
        mSymbols.Zero();
        NS_WARNING("GLContext::InitWithPrefix failed!");
        return false;
    }

    reporter.SetSuccessful();
    return true;
}

static bool
LoadGLSymbols(GLContext* gl, const char* prefix, bool trygl,
              const GLLibraryLoader::SymLoadStruct* list, const char* desc)
{
    if (gl->LoadSymbols(list, trygl, prefix))
        return true;

    ClearSymbols(list);

    if (desc) {
        const nsPrintfCString err("Failed to load symbols for %s.", desc);
        NS_ERROR(err.BeginReading());
    }
    return false;
}

bool
GLContext::LoadExtSymbols(const char* prefix, bool trygl, const SymLoadStruct* list,
                          GLExtensions ext)
{
    const char* extName = sExtensionNames[size_t(ext)];
    if (!LoadGLSymbols(this, prefix, trygl, list, extName)) {
        MarkExtensionUnsupported(ext);
        return false;
    }
    return true;
};

bool
GLContext::LoadFeatureSymbols(const char* prefix, bool trygl, const SymLoadStruct* list,
                              GLFeature feature)
{
    const char* featureName = GetFeatureName(feature);
    if (!LoadGLSymbols(this, prefix, trygl, list, featureName)) {
        MarkUnsupported(feature);
        return false;
    }
    return true;
};

bool
GLContext::InitWithPrefixImpl(const char* prefix, bool trygl)
{
    mWorkAroundDriverBugs = gfxPrefs::WorkAroundDriverBugs();

    const SymLoadStruct coreSymbols[] = {
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

        { (PRFuncPtr*) &mSymbols.fGenBuffers, { "GenBuffers", "GenBuffersARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fGenTextures, { "GenTextures", nullptr } },
        { (PRFuncPtr*) &mSymbols.fCreateProgram, { "CreateProgram", "CreateProgramARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fCreateShader, { "CreateShader", "CreateShaderARB", nullptr } },

        { (PRFuncPtr*) &mSymbols.fDeleteBuffers, { "DeleteBuffers", "DeleteBuffersARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDeleteTextures, { "DeleteTextures", "DeleteTexturesARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDeleteProgram, { "DeleteProgram", "DeleteProgramARB", nullptr } },
        { (PRFuncPtr*) &mSymbols.fDeleteShader, { "DeleteShader", "DeleteShaderARB", nullptr } },

        END_SYMBOLS
    };

    if (!LoadGLSymbols(this, prefix, trygl, coreSymbols, "GL"))
        return false;

    ////////////////

    MakeCurrent();
    MOZ_ASSERT(mProfile != ContextProfile::Unknown);

    uint32_t version = 0;
    ParseGLVersion(this, &version);

    mShadingLanguageVersion = 100;
    ParseGLSLVersion(this, &mShadingLanguageVersion);

    if (ShouldSpew()) {
        printf_stderr("OpenGL version detected: %u\n", version);
        printf_stderr("OpenGL shading language version detected: %u\n", mShadingLanguageVersion);
        printf_stderr("OpenGL vendor: %s\n", fGetString(LOCAL_GL_VENDOR));
        printf_stderr("OpenGL renderer: %s\n", fGetString(LOCAL_GL_RENDERER));
    }

    if (version >= mVersion) {
        mVersion = version;
    }
    // Don't fail if version < mVersion, see bug 999445,
    // Mac OSX 10.6/10.7 machines with Intel GPUs claim only OpenGL 1.4 but
    // have all the GL2+ extensions that we need.

    ////////////////

    // Load OpenGL ES 2.0 symbols, or desktop if we aren't using ES 2.
    if (IsGLES()) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetShaderPrecisionFormat, { "GetShaderPrecisionFormat", nullptr } },
            { (PRFuncPtr*) &mSymbols.fClearDepthf, { "ClearDepthf", nullptr } },
            { (PRFuncPtr*) &mSymbols.fDepthRangef, { "DepthRangef", nullptr } },
            END_SYMBOLS
        };

        if (!LoadGLSymbols(this, prefix, trygl, symbols, "OpenGL ES"))
            return false;
    } else {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fClearDepth, { "ClearDepth", nullptr } },
            { (PRFuncPtr*) &mSymbols.fDepthRange, { "DepthRange", nullptr } },
            { (PRFuncPtr*) &mSymbols.fReadBuffer, { "ReadBuffer", nullptr } },
            { (PRFuncPtr*) &mSymbols.fMapBuffer, { "MapBuffer", nullptr } },
            { (PRFuncPtr*) &mSymbols.fUnmapBuffer, { "UnmapBuffer", nullptr } },
            { (PRFuncPtr*) &mSymbols.fPointParameterf, { "PointParameterf", nullptr } },
            { (PRFuncPtr*) &mSymbols.fDrawBuffer, { "DrawBuffer", nullptr } },
            // The following functions are only used by Skia/GL in desktop mode.
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

        if (!LoadGLSymbols(this, prefix, trygl, symbols, "Desktop OpenGL"))
            return false;
    }

    ////////////////

    const char* glVendorString = (const char*)fGetString(LOCAL_GL_VENDOR);
    const char* glRendererString = (const char*)fGetString(LOCAL_GL_RENDERER);
    if (!glVendorString || !glRendererString)
        return false;

    // The order of these strings must match up with the order of the enum
    // defined in GLContext.h for vendor IDs.
    const char* vendorMatchStrings[size_t(GLVendor::Other)] = {
        "Intel",
        "NVIDIA",
        "ATI",
        "Qualcomm",
        "Imagination",
        "nouveau",
        "Vivante",
        "VMware, Inc.",
        "ARM"
    };

    mVendor = GLVendor::Other;
    for (size_t i = 0; i < size_t(GLVendor::Other); ++i) {
        if (DoesStringMatch(glVendorString, vendorMatchStrings[i])) {
            mVendor = GLVendor(i);
            break;
        }
    }

    // The order of these strings must match up with the order of the enum
    // defined in GLContext.h for renderer IDs.
    const char* rendererMatchStrings[size_t(GLRenderer::Other)] = {
        "Adreno 200",
        "Adreno 205",
        "Adreno (TM) 200",
        "Adreno (TM) 205",
        "Adreno (TM) 305",
        "Adreno (TM) 320",
        "Adreno (TM) 330",
        "Adreno (TM) 420",
        "Mali-400 MP",
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

    if (ShouldSpew()) {
        const char* vendors[size_t(GLVendor::Other)] = {
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
            printf_stderr("OpenGL vendor ('%s') not recognized.\n", glVendorString);
        }
    }

    ////////////////

    // We need this for retrieving the list of extensions on Core profiles.
    if (IsFeatureProvidedByCoreSymbols(GLFeature::get_string_indexed)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetStringi, { "GetStringi", nullptr } },
            END_SYMBOLS
        };

        if (!LoadGLSymbols(this, prefix, trygl, symbols, "get_string_indexed")) {
            MOZ_RELEASE_ASSERT(false, "GFX: get_string_indexed is required!");
            return false;
        }
    }

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

        if (Renderer() == GLRenderer::MicrosoftBasicRenderDriver) {
            // Bug 978966: on Microsoft's "Basic Render Driver" (software renderer)
            // multisampling hardcodes blending with the default blendfunc, which breaks WebGL.
            MarkUnsupported(GLFeature::framebuffer_multisample);
        }

#ifdef XP_MACOSX
        // The Mac Nvidia driver, for versions up to and including 10.8,
        // don't seem to properly support this.  See 814839
        // this has been fixed in Mac OS X 10.9. See 907946
        // and it also works in 10.8.3 and higher.  See 1094338.
        if (Vendor() == gl::GLVendor::NVIDIA &&
            !nsCocoaFeatures::IsAtLeastVersion(10,8,3))
        {
            MarkUnsupported(GLFeature::depth_texture);
        }
#endif
    }

    if (IsExtensionSupported(GLContext::ARB_pixel_buffer_object)) {
        MOZ_ASSERT((mSymbols.fMapBuffer && mSymbols.fUnmapBuffer),
                   "ARB_pixel_buffer_object supported without glMapBuffer/UnmapBuffer"
                   " being available!");
    }

    ////////////////////////////////////////////////////////////////////////////

    const auto fnLoadForFeature = [this, prefix, trygl](const SymLoadStruct* list,
                                                        GLFeature feature)
    {
        return this->LoadFeatureSymbols(prefix, trygl, list, feature);
    };

    // Check for ARB_framebuffer_objects
    if (IsSupported(GLFeature::framebuffer_object)) {
        // https://www.opengl.org/registry/specs/ARB/framebuffer_object.txt
        const SymLoadStruct symbols[] = {
            CORE_SYMBOL(IsRenderbuffer),
            CORE_SYMBOL(BindRenderbuffer),
            CORE_SYMBOL(DeleteRenderbuffers),
            CORE_SYMBOL(GenRenderbuffers),
            CORE_SYMBOL(RenderbufferStorage),
            CORE_SYMBOL(RenderbufferStorageMultisample),
            CORE_SYMBOL(GetRenderbufferParameteriv),
            CORE_SYMBOL(IsFramebuffer),
            CORE_SYMBOL(BindFramebuffer),
            CORE_SYMBOL(DeleteFramebuffers),
            CORE_SYMBOL(GenFramebuffers),
            CORE_SYMBOL(CheckFramebufferStatus),
            CORE_SYMBOL(FramebufferTexture2D),
            CORE_SYMBOL(FramebufferTextureLayer),
            CORE_SYMBOL(FramebufferRenderbuffer),
            CORE_SYMBOL(GetFramebufferAttachmentParameteriv),
            CORE_SYMBOL(BlitFramebuffer),
            CORE_SYMBOL(GenerateMipmap),
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::framebuffer_object);
    }

    if (!IsSupported(GLFeature::framebuffer_object)) {
        // Check for aux symbols based on extensions
        if (IsSupported(GLFeature::framebuffer_object_EXT_OES)) {
            const SymLoadStruct symbols[] = {
                CORE_EXT_SYMBOL2(IsRenderbuffer, EXT, OES),
                CORE_EXT_SYMBOL2(BindRenderbuffer, EXT, OES),
                CORE_EXT_SYMBOL2(DeleteRenderbuffers, EXT, OES),
                CORE_EXT_SYMBOL2(GenRenderbuffers, EXT, OES),
                CORE_EXT_SYMBOL2(RenderbufferStorage, EXT, OES),
                CORE_EXT_SYMBOL2(GetRenderbufferParameteriv, EXT, OES),
                CORE_EXT_SYMBOL2(IsFramebuffer, EXT, OES),
                CORE_EXT_SYMBOL2(BindFramebuffer, EXT, OES),
                CORE_EXT_SYMBOL2(DeleteFramebuffers, EXT, OES),
                CORE_EXT_SYMBOL2(GenFramebuffers, EXT, OES),
                CORE_EXT_SYMBOL2(CheckFramebufferStatus, EXT, OES),
                CORE_EXT_SYMBOL2(FramebufferTexture2D, EXT, OES),
                CORE_EXT_SYMBOL2(FramebufferRenderbuffer, EXT, OES),
                CORE_EXT_SYMBOL2(GetFramebufferAttachmentParameteriv, EXT, OES),
                CORE_EXT_SYMBOL2(GenerateMipmap, EXT, OES),
                END_SYMBOLS
            };
            fnLoadForFeature(symbols, GLFeature::framebuffer_object_EXT_OES);
        }

        if (IsSupported(GLFeature::framebuffer_blit)) {
            const SymLoadStruct symbols[] = {
                EXT_SYMBOL3(BlitFramebuffer, ANGLE, EXT, NV),
                END_SYMBOLS
            };
            fnLoadForFeature(symbols, GLFeature::framebuffer_blit);
        }

        if (IsSupported(GLFeature::framebuffer_multisample)) {
            const SymLoadStruct symbols[] = {
                EXT_SYMBOL3(RenderbufferStorageMultisample, ANGLE, APPLE, EXT),
                END_SYMBOLS
            };
            fnLoadForFeature(symbols, GLFeature::framebuffer_multisample);
        }

        if (IsExtensionSupported(GLContext::ARB_geometry_shader4) ||
            IsExtensionSupported(GLContext::NV_geometry_program4))
        {
            const SymLoadStruct symbols[] = {
                EXT_SYMBOL2(FramebufferTextureLayer, ARB, EXT),
                END_SYMBOLS
            };
            if (!LoadGLSymbols(this, prefix, trygl, symbols,
                               "ARB_geometry_shader4/NV_geometry_program4"))
            {
                MarkExtensionUnsupported(GLContext::ARB_geometry_shader4);
                MarkExtensionUnsupported(GLContext::NV_geometry_program4);
            }
        }
    }

    if (!IsSupported(GLFeature::framebuffer_object) &&
        !IsSupported(GLFeature::framebuffer_object_EXT_OES))
    {
        NS_ERROR("GLContext requires support for framebuffer objects.");
        return false;
    }
    MOZ_RELEASE_ASSERT(mSymbols.fBindFramebuffer, "GFX: mSymbols.fBindFramebuffer zero or not set.");

    ////////////////

    LoadMoreSymbols(prefix, trygl);

    ////////////////////////////////////////////////////////////////////////////

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
            // See bug 879656.  8192 fails, 8191 works.
            mMaxTextureSize = std::min(mMaxTextureSize, 8191);
            mMaxRenderbufferSize = std::min(mMaxRenderbufferSize, 8191);

            // Part of the bug 879656, but it also doesn't hurt the 877949
            mNeedsTextureSizeChecks = true;
        }
    }
#endif
#ifdef MOZ_X11
    if (mWorkAroundDriverBugs) {
        if (mVendor == GLVendor::Nouveau) {
            // see bug 814716. Clamp MaxCubeMapTextureSize at 2K for Nouveau.
            mMaxCubeMapTextureSize = std::min(mMaxCubeMapTextureSize, 2048);
            mNeedsTextureSizeChecks = true;
        } else if (mVendor == GLVendor::Intel) {
            // Bug 1199923. Driver seems to report a larger max size than
            // actually supported.
            mMaxTextureSize /= 2;
            mMaxRenderbufferSize /= 2;
            mNeedsTextureSizeChecks = true;
        }
    }
#endif
    if (mWorkAroundDriverBugs &&
        Renderer() == GLRenderer::AdrenoTM420) {
        // see bug 1194923. Calling glFlush before glDeleteFramebuffers
        // prevents occasional driver crash.
        mNeedsFlushBeforeDeleteFB = true;
    }
#ifdef MOZ_WIDGET_ANDROID
    if (mWorkAroundDriverBugs &&
        (Renderer() == GLRenderer::AdrenoTM305 ||
         Renderer() == GLRenderer::AdrenoTM320 ||
         Renderer() == GLRenderer::AdrenoTM330) &&
        AndroidBridge::Bridge()->GetAPIVersion() < 21) {
        // Bug 1164027. Driver crashes when functions such as
        // glTexImage2D fail due to virtual memory exhaustion.
        mTextureAllocCrashesOnMapFailure = true;
    }
#endif

    mMaxTextureImageSize = mMaxTextureSize;

    if (IsSupported(GLFeature::framebuffer_multisample)) {
        fGetIntegerv(LOCAL_GL_MAX_SAMPLES, (GLint*)&mMaxSamples);
    }

    ////////////////////////////////////////////////////////////////////////////

    // We're ready for final setup.
    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

    // TODO: Remove SurfaceCaps::any.
    if (mCaps.any) {
        mCaps.any = false;
        mCaps.color = true;
        mCaps.alpha = false;
    }

    mTexGarbageBin = new TextureGarbageBin(this);

    MOZ_ASSERT(IsCurrent());

    if (ShouldSpew() && IsExtensionSupported(KHR_debug)) {
        fEnable(LOCAL_GL_DEBUG_OUTPUT);
        fDisable(LOCAL_GL_DEBUG_OUTPUT_SYNCHRONOUS);
        fDebugMessageCallback(&StaticDebugCallback, (void*)this);
        fDebugMessageControl(LOCAL_GL_DONT_CARE,
                             LOCAL_GL_DONT_CARE,
                             LOCAL_GL_DONT_CARE,
                             0, nullptr,
                             true);
    }

    mVersionString = nsPrintfCString("%u.%u.%u", mVersion / 100, (mVersion / 10) % 10,
                                     mVersion % 10);
    return true;
}

void
GLContext::LoadMoreSymbols(const char* prefix, bool trygl)
{
    const auto fnLoadForExt = [this, prefix, trygl](const SymLoadStruct* list,
                                                    GLExtensions ext)
    {
        return this->LoadExtSymbols(prefix, trygl, list, ext);
    };

    const auto fnLoadForFeature = [this, prefix, trygl](const SymLoadStruct* list,
                                                        GLFeature feature)
    {
        return this->LoadFeatureSymbols(prefix, trygl, list, feature);
    };

    const auto fnLoadFeatureByCore = [this, fnLoadForFeature](const SymLoadStruct* coreList,
                                                              const SymLoadStruct* extList,
                                                              GLFeature feature)
    {
        const bool useCore = this->IsFeatureProvidedByCoreSymbols(feature);
        const auto list = useCore ? coreList : extList;
        return fnLoadForFeature(list, feature);
    };

    if (IsSupported(GLFeature::robustness)) {
        bool hasRobustness = false;

        if (!hasRobustness && IsExtensionSupported(ARB_robustness)) {
            const SymLoadStruct symbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetGraphicsResetStatus, { "GetGraphicsResetStatusARB", nullptr } },
                END_SYMBOLS
            };
            if (fnLoadForExt(symbols, ARB_robustness)) {
                hasRobustness = true;
            }
        }

        if (!hasRobustness && IsExtensionSupported(EXT_robustness)) {
            const SymLoadStruct symbols[] = {
                { (PRFuncPtr*) &mSymbols.fGetGraphicsResetStatus, { "GetGraphicsResetStatusEXT", nullptr } },
                END_SYMBOLS
            };
            if (fnLoadForExt(symbols, EXT_robustness)) {
                hasRobustness = true;
            }
        }

        if (!hasRobustness) {
            MarkUnsupported(GLFeature::robustness);
        }
    }

    if (IsSupported(GLFeature::sync)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fFenceSync,      { "FenceSync",      nullptr } },
            { (PRFuncPtr*) &mSymbols.fIsSync,         { "IsSync",         nullptr } },
            { (PRFuncPtr*) &mSymbols.fDeleteSync,     { "DeleteSync",     nullptr } },
            { (PRFuncPtr*) &mSymbols.fClientWaitSync, { "ClientWaitSync", nullptr } },
            { (PRFuncPtr*) &mSymbols.fWaitSync,       { "WaitSync",       nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetInteger64v,  { "GetInteger64v",  nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetSynciv,      { "GetSynciv",      nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::sync);
    }

    if (IsExtensionSupported(OES_EGL_image)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fEGLImageTargetTexture2D, { "EGLImageTargetTexture2DOES", nullptr } },
            { (PRFuncPtr*) &mSymbols.fEGLImageTargetRenderbufferStorage, { "EGLImageTargetRenderbufferStorageOES", nullptr } },
            END_SYMBOLS
        };
        fnLoadForExt(symbols, OES_EGL_image);
    }

    if (IsExtensionSupported(APPLE_texture_range)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fTextureRangeAPPLE, { "TextureRangeAPPLE", nullptr } },
            END_SYMBOLS
        };
        fnLoadForExt(symbols, APPLE_texture_range);
    }

    if (IsSupported(GLFeature::vertex_array_object)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fIsVertexArray, { "IsVertexArray", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGenVertexArrays, { "GenVertexArrays", nullptr } },
            { (PRFuncPtr*) &mSymbols.fBindVertexArray, { "BindVertexArray", nullptr } },
            { (PRFuncPtr*) &mSymbols.fDeleteVertexArrays, { "DeleteVertexArrays", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fIsVertexArray, { "IsVertexArrayARB", "IsVertexArrayOES", "IsVertexArrayAPPLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGenVertexArrays, { "GenVertexArraysARB", "GenVertexArraysOES", "GenVertexArraysAPPLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fBindVertexArray, { "BindVertexArrayARB", "BindVertexArrayOES", "BindVertexArrayAPPLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fDeleteVertexArrays, { "DeleteVertexArraysARB", "DeleteVertexArraysOES", "DeleteVertexArraysAPPLE", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::vertex_array_object);
    }

    if (IsSupported(GLFeature::draw_instanced)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawArraysInstanced, { "DrawArraysInstanced", nullptr } },
            { (PRFuncPtr*) &mSymbols.fDrawElementsInstanced, { "DrawElementsInstanced", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawArraysInstanced, { "DrawArraysInstancedARB", "DrawArraysInstancedEXT", "DrawArraysInstancedNV", "DrawArraysInstancedANGLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fDrawElementsInstanced, { "DrawElementsInstancedARB", "DrawElementsInstancedEXT", "DrawElementsInstancedNV", "DrawElementsInstancedANGLE", nullptr }
            },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::draw_instanced);
    }

    if (IsSupported(GLFeature::instanced_arrays)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fVertexAttribDivisor, { "VertexAttribDivisor", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fVertexAttribDivisor, { "VertexAttribDivisorARB", "VertexAttribDivisorNV", "VertexAttribDivisorANGLE", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::instanced_arrays);
    }

    if (IsSupported(GLFeature::texture_storage)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fTexStorage2D, { "TexStorage2D", nullptr } },
            { (PRFuncPtr*) &mSymbols.fTexStorage3D, { "TexStorage3D", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fTexStorage2D, { "TexStorage2DEXT", nullptr } },
            { (PRFuncPtr*) &mSymbols.fTexStorage3D, { "TexStorage3DEXT", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_storage);
    }

    if (IsSupported(GLFeature::sampler_objects)) {
        const SymLoadStruct symbols[] = {
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
        fnLoadForFeature(symbols, GLFeature::sampler_objects);
    }

    // ARB_transform_feedback2/NV_transform_feedback2 is a
    // superset of EXT_transform_feedback/NV_transform_feedback
    // and adds glPauseTransformFeedback &
    // glResumeTransformFeedback, which are required for WebGL2.
    if (IsSupported(GLFeature::transform_feedback2)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBindBufferBase, { "BindBufferBase", nullptr } },
            { (PRFuncPtr*) &mSymbols.fBindBufferRange, { "BindBufferRange", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGenTransformFeedbacks, { "GenTransformFeedbacks", nullptr } },
            { (PRFuncPtr*) &mSymbols.fBindTransformFeedback, { "BindTransformFeedback", nullptr } },
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
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBindBufferBase, { "BindBufferBaseEXT", "BindBufferBaseNV", nullptr } },
            { (PRFuncPtr*) &mSymbols.fBindBufferRange, { "BindBufferRangeEXT", "BindBufferRangeNV", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGenTransformFeedbacks, { "GenTransformFeedbacksNV", nullptr } },
            { (PRFuncPtr*) &mSymbols.fBindTransformFeedback, { "BindTransformFeedbackNV", nullptr } },
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
        if (!fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_storage)) {
            // Also mark bind_buffer_offset as unsupported.
            MarkUnsupported(GLFeature::bind_buffer_offset);
        }
    }

    if (IsSupported(GLFeature::bind_buffer_offset)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBindBufferOffset, { "BindBufferOffset", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBindBufferOffset,
              { "BindBufferOffsetEXT", "BindBufferOffsetNV", nullptr }
            },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::bind_buffer_offset);
    }

    if (IsSupported(GLFeature::query_counter)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fQueryCounter, { "QueryCounter", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fQueryCounter, { "QueryCounterEXT", "QueryCounterANGLE", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::query_counter);
    }

    if (IsSupported(GLFeature::query_objects)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBeginQuery, { "BeginQuery", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGenQueries, { "GenQueries", nullptr } },
            { (PRFuncPtr*) &mSymbols.fDeleteQueries, { "DeleteQueries", nullptr } },
            { (PRFuncPtr*) &mSymbols.fEndQuery, { "EndQuery", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetQueryiv, { "GetQueryiv", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectuiv, { "GetQueryObjectuiv", nullptr } },
            { (PRFuncPtr*) &mSymbols.fIsQuery, { "IsQuery", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBeginQuery, { "BeginQueryEXT", "BeginQueryANGLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGenQueries, { "GenQueriesEXT", "GenQueriesANGLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fDeleteQueries, { "DeleteQueriesEXT", "DeleteQueriesANGLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fEndQuery, { "EndQueryEXT", "EndQueryANGLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetQueryiv, { "GetQueryivEXT", "GetQueryivANGLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectuiv, { "GetQueryObjectuivEXT", "GetQueryObjectuivANGLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fIsQuery, { "IsQueryEXT", "IsQueryANGLE", nullptr } },
            END_SYMBOLS
        };
        if (!fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::query_objects)) {
            MarkUnsupported(GLFeature::get_query_object_i64v);
            MarkUnsupported(GLFeature::get_query_object_iv);
            MarkUnsupported(GLFeature::occlusion_query);
            MarkUnsupported(GLFeature::occlusion_query_boolean);
            MarkUnsupported(GLFeature::occlusion_query2);
        }
    }

    if (IsSupported(GLFeature::get_query_object_i64v)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetQueryObjecti64v, { "GetQueryObjecti64v", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectui64v, { "GetQueryObjectui64v", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetQueryObjecti64v, { "GetQueryObjecti64vEXT", "GetQueryObjecti64vANGLE", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectui64v, { "GetQueryObjectui64vEXT", "GetQueryObjectui64vANGLE", nullptr } },
            END_SYMBOLS
        };
        if (!fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::get_query_object_i64v)) {
            MarkUnsupported(GLFeature::query_counter);
        }
    }

    if (IsSupported(GLFeature::get_query_object_iv)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectiv, { "GetQueryObjectiv", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectiv, { "GetQueryObjectivEXT", "GetQueryObjectivANGLE", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::get_query_object_iv);
    }

    if (IsSupported(GLFeature::clear_buffers)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fClearBufferfi,  { "ClearBufferfi",  nullptr } },
            { (PRFuncPtr*) &mSymbols.fClearBufferfv,  { "ClearBufferfv",  nullptr } },
            { (PRFuncPtr*) &mSymbols.fClearBufferiv,  { "ClearBufferiv",  nullptr } },
            { (PRFuncPtr*) &mSymbols.fClearBufferuiv, { "ClearBufferuiv", nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::clear_buffers);
    }

    if (IsSupported(GLFeature::copy_buffer)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fCopyBufferSubData, { "CopyBufferSubData", nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::copy_buffer);
    }

    if (IsSupported(GLFeature::draw_buffers)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawBuffers, { "DrawBuffers", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawBuffers, { "DrawBuffersARB", "DrawBuffersEXT", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::draw_buffers);
    }

    if (IsSupported(GLFeature::draw_range_elements)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawRangeElements, { "DrawRangeElements", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawRangeElements, { "DrawRangeElementsEXT", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::draw_range_elements);
    }

    if (IsSupported(GLFeature::get_integer_indexed)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetIntegeri_v, { "GetIntegeri_v", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] ={
            { (PRFuncPtr*) &mSymbols.fGetIntegeri_v, { "GetIntegerIndexedvEXT", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::get_integer_indexed);
    }

    if (IsSupported(GLFeature::get_integer64_indexed)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetInteger64i_v, { "GetInteger64i_v", nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::get_integer64_indexed);
    }

    if (IsSupported(GLFeature::gpu_shader4)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetVertexAttribIiv, { "GetVertexAttribIiv", "GetVertexAttribIivEXT", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetVertexAttribIuiv, { "GetVertexAttribIuiv", "GetVertexAttribIuivEXT", nullptr } },
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
            { (PRFuncPtr*) &mSymbols.fGetUniformuiv, { "GetUniformuiv", "GetUniformuivEXT", nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::gpu_shader4);
    }

    if (IsSupported(GLFeature::map_buffer_range)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fMapBufferRange, { "MapBufferRange", nullptr } },
            { (PRFuncPtr*) &mSymbols.fFlushMappedBufferRange, { "FlushMappedBufferRange", nullptr } },
            { (PRFuncPtr*) &mSymbols.fUnmapBuffer, { "UnmapBuffer", nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::map_buffer_range);
    }

    if (IsSupported(GLFeature::texture_3D)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fTexImage3D, { "TexImage3D", nullptr } },
            { (PRFuncPtr*) &mSymbols.fTexSubImage3D, { "TexSubImage3D", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fTexSubImage3D, { "TexSubImage3DEXT", "TexSubImage3DOES", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_3D);
    }

    if (IsSupported(GLFeature::texture_3D_compressed)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCompressedTexImage3D, { "CompressedTexImage3D", nullptr } },
            { (PRFuncPtr*) &mSymbols.fCompressedTexSubImage3D, { "CompressedTexSubImage3D", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCompressedTexImage3D, { "CompressedTexImage3DARB", "CompressedTexImage3DOES", nullptr } },
            { (PRFuncPtr*) &mSymbols.fCompressedTexSubImage3D, { "CompressedTexSubImage3DARB", "CompressedTexSubImage3DOES", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_3D_compressed);
    }

    if (IsSupported(GLFeature::texture_3D_copy)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCopyTexSubImage3D, { "CopyTexSubImage3D", nullptr } },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCopyTexSubImage3D, { "CopyTexSubImage3DEXT", "CopyTexSubImage3DOES", nullptr } },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_3D_copy);
    }

    if (IsSupported(GLFeature::uniform_buffer_object)) {
        // Note: Don't query for glGetActiveUniformName because it is not
        // supported by GL ES 3.
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetUniformIndices, { "GetUniformIndices", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetActiveUniformsiv, { "GetActiveUniformsiv", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetUniformBlockIndex, { "GetUniformBlockIndex", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetActiveUniformBlockiv, { "GetActiveUniformBlockiv", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetActiveUniformBlockName, { "GetActiveUniformBlockName", nullptr } },
            { (PRFuncPtr*) &mSymbols.fUniformBlockBinding, { "UniformBlockBinding", nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::uniform_buffer_object);
    }

    if (IsSupported(GLFeature::uniform_matrix_nonsquare)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fUniformMatrix2x3fv, { "UniformMatrix2x3fv", nullptr } },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix2x4fv, { "UniformMatrix2x4fv", nullptr } },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix3x2fv, { "UniformMatrix3x2fv", nullptr } },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix3x4fv, { "UniformMatrix3x4fv", nullptr } },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix4x2fv, { "UniformMatrix4x2fv", nullptr } },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix4x3fv, { "UniformMatrix4x3fv", nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::uniform_matrix_nonsquare);
    }

    if (IsSupported(GLFeature::internalformat_query)) {
        const SymLoadStruct symbols[] = {
            CORE_SYMBOL(GetInternalformativ),
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::internalformat_query);
    }

    if (IsSupported(GLFeature::invalidate_framebuffer)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fInvalidateFramebuffer,    { "InvalidateFramebuffer", nullptr } },
            { (PRFuncPtr*) &mSymbols.fInvalidateSubFramebuffer, { "InvalidateSubFramebuffer", nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::invalidate_framebuffer);
    }

    if (IsExtensionSupported(KHR_debug)) {
        const SymLoadStruct symbols[] = {
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
        fnLoadForExt(symbols, KHR_debug);
    }

    if (IsExtensionSupported(NV_fence)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGenFences,    { "GenFencesNV",    nullptr } },
            { (PRFuncPtr*) &mSymbols.fDeleteFences, { "DeleteFencesNV", nullptr } },
            { (PRFuncPtr*) &mSymbols.fSetFence,     { "SetFenceNV",     nullptr } },
            { (PRFuncPtr*) &mSymbols.fTestFence,    { "TestFenceNV",    nullptr } },
            { (PRFuncPtr*) &mSymbols.fFinishFence,  { "FinishFenceNV",  nullptr } },
            { (PRFuncPtr*) &mSymbols.fIsFence,      { "IsFenceNV",      nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetFenceiv,   { "GetFenceivNV",   nullptr } },
            END_SYMBOLS
        };
        fnLoadForExt(symbols, NV_fence);
    }

    if (IsExtensionSupported(NV_texture_barrier)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fTextureBarrier, { "TextureBarrierNV", nullptr } },
            END_SYMBOLS
        };
        fnLoadForExt(symbols, NV_texture_barrier);
    }

    if (IsSupported(GLFeature::read_buffer)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fReadBuffer, { "ReadBuffer", nullptr } },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::read_buffer);
    }

    if (IsExtensionSupported(APPLE_framebuffer_multisample)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fResolveMultisampleFramebufferAPPLE, { "ResolveMultisampleFramebufferAPPLE", nullptr } },
            END_SYMBOLS
        };
        fnLoadForExt(symbols, APPLE_framebuffer_multisample);
    }

    // Load developer symbols, don't fail if we can't find them.
    const SymLoadStruct devSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetTexImage, { "GetTexImage", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetTexLevelParameteriv, { "GetTexLevelParameteriv", nullptr } },
            END_SYMBOLS
    };
    const bool warnOnFailures = ShouldSpew();
    LoadSymbols(devSymbols, trygl, prefix, warnOnFailures);
}

#undef CORE_SYMBOL
#undef CORE_EXT_SYMBOL2
#undef EXT_SYMBOL2
#undef EXT_SYMBOL3
#undef END_SYMBOLS

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

    printf_stderr("[KHR_debug: 0x%" PRIxPTR "] ID %u: %s, %s, %s:\n    %s\n",
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
    MOZ_ASSERT(IsCurrent());

    std::vector<nsCString> driverExtensionList;

    if (IsFeatureProvidedByCoreSymbols(GLFeature::get_string_indexed)) {
        GLuint count = 0;
        GetUIntegerv(LOCAL_GL_NUM_EXTENSIONS, &count);
        for (GLuint i = 0; i < count; i++) {
            // This is UTF-8.
            const char* rawExt = (const char*)fGetStringi(LOCAL_GL_EXTENSIONS, i);

            // We CANNOT use nsDependentCString here, because the spec doesn't guarantee
            // that the pointers returned are different, only that their contents are.
            // On Flame, each of these index string queries returns the same address.
            driverExtensionList.push_back(nsCString(rawExt));
        }
    } else {
        MOZ_ALWAYS_TRUE(!fGetError());
        const char* rawExts = (const char*)fGetString(LOCAL_GL_EXTENSIONS);
        MOZ_ALWAYS_TRUE(!fGetError());

        if (rawExts) {
            nsDependentCString exts(rawExts);
            SplitByChar(exts, ' ', &driverExtensionList);
        }
    }

    const bool shouldDumpExts = ShouldDumpExts();
    if (shouldDumpExts) {
        printf_stderr("%i GL driver extensions: (*: recognized)\n",
                      (uint32_t)driverExtensionList.size());
    }

    MarkBitfieldByStrings(driverExtensionList, shouldDumpExts, sExtensionNames,
                          &mAvailableExtensions);

    if (WorkAroundDriverBugs()) {
        if (Vendor() == GLVendor::Qualcomm) {
            // Some Adreno drivers do not report GL_OES_EGL_sync, but they really do support it.
            MarkExtensionSupported(OES_EGL_sync);
        }

        if (Vendor() == GLVendor::Imagination &&
            Renderer() == GLRenderer::SGX540)
        {
            // Bug 980048
            MarkExtensionUnsupported(OES_EGL_sync);
        }

        if (Vendor() == GLVendor::ARM &&
            Renderer() == GLRenderer::Mali400MP)
        {
            // Bug 1264505
            MarkExtensionUnsupported(OES_EGL_image_external);
        }

        if (Renderer() == GLRenderer::AndroidEmulator) {
            // the Android emulator, which we use to run B2G reftests on,
            // doesn't expose the OES_rgb8_rgba8 extension, but it seems to
            // support it (tautologically, as it only runs on desktop GL).
            MarkExtensionSupported(OES_rgb8_rgba8);
            // there seems to be a similar issue for EXT_texture_format_BGRA8888
            // on the Android 4.3 emulator
            MarkExtensionSupported(EXT_texture_format_BGRA8888);
        }

        if (Vendor() == GLVendor::VMware &&
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
        // Bug 1124996: Appears to be the same on OSX Yosemite (10.10)
        if (nsCocoaFeatures::OSXVersionMajor() == 10 &&
            nsCocoaFeatures::OSXVersionMinor() >= 9 &&
            Renderer() == GLRenderer::IntelHD3000)
        {
            MarkExtensionUnsupported(EXT_texture_compression_s3tc);
        }
#endif
    }

    if (shouldDumpExts) {
        printf_stderr("\nActivated extensions:\n");

        for (size_t i = 0; i < mAvailableExtensions.size(); i++) {
            if (!mAvailableExtensions[i])
                continue;

            const char* ext = sExtensionNames[i];
            printf_stderr("[%i] %s\n", (uint32_t)i, ext);
        }
    }
}

void
GLContext::PlatformStartup()
{
    RegisterStrongMemoryReporter(new GfxTexturesReporter());
}

// Common code for checking for both GL extensions and GLX extensions.
bool
GLContext::ListHasExtension(const GLubyte* extensions, const char* extension)
{
    // fix bug 612572 - we were crashing as we were calling this function with extensions==null
    if (extensions == nullptr || extension == nullptr)
        return false;

    const GLubyte* start;
    GLubyte* where;
    GLubyte* terminator;

    /* Extension names should not have spaces. */
    where = (GLubyte*) strchr(extension, ' ');
    if (where || *extension == '\0')
        return false;

    /*
     * It takes a bit of care to be fool-proof about parsing the
     * OpenGL extensions string. Don't be fooled by sub-strings,
     * etc.
     */
    start = extensions;
    for (;;) {
        where = (GLubyte*) strstr((const char*) start, extension);
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
    if (IsSupported(GLFeature::packed_depth_stencil)) {
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
        // On the Android 4.3 emulator, IsRenderbuffer may return false incorrectly.
        MOZ_ASSERT_IF(Renderer() != GLRenderer::AndroidEmulator, fIsRenderbuffer(colorRB));
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_COLOR_ATTACHMENT0,
                                 LOCAL_GL_RENDERBUFFER,
                                 colorRB);
    }

    if (depthRB) {
        MOZ_ASSERT_IF(Renderer() != GLRenderer::AndroidEmulator, fIsRenderbuffer(depthRB));
        fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER,
                                 depthRB);
    }

    if (stencilRB) {
        MOZ_ASSERT_IF(Renderer() != GLRenderer::AndroidEmulator, fIsRenderbuffer(stencilRB));
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
        if (ShouldSpew()) {
            printf_stderr("Framebuffer status: %X\n", status);
        }
  #endif
        isComplete = false;
    }

    if (!IsFramebufferComplete(readFB, &status)) {
        NS_WARNING("ReadFBO: Incomplete");
  #ifdef MOZ_GL_DEBUG
        if (ShouldSpew()) {
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

    // Null these before they're naturally nulled after dtor, as we want GLContext to
    // still be alive in *their* dtors.
    mScreen = nullptr;
    mBlitHelper = nullptr;
    mReadTexImageHelper = nullptr;

    if (MakeCurrent()) {
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
GLContext::CreatedProgram(GLContext* aOrigin, GLuint aName)
{
    mTrackedPrograms.AppendElement(NamedResource(aOrigin, aName));
}

void
GLContext::CreatedShader(GLContext* aOrigin, GLuint aName)
{
    mTrackedShaders.AppendElement(NamedResource(aOrigin, aName));
}

void
GLContext::CreatedBuffers(GLContext* aOrigin, GLsizei aCount, GLuint* aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedBuffers.AppendElement(NamedResource(aOrigin, aNames[i]));
    }
}

void
GLContext::CreatedQueries(GLContext* aOrigin, GLsizei aCount, GLuint* aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedQueries.AppendElement(NamedResource(aOrigin, aNames[i]));
    }
}

void
GLContext::CreatedTextures(GLContext* aOrigin, GLsizei aCount, GLuint* aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedTextures.AppendElement(NamedResource(aOrigin, aNames[i]));
    }
}

void
GLContext::CreatedFramebuffers(GLContext* aOrigin, GLsizei aCount, GLuint* aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedFramebuffers.AppendElement(NamedResource(aOrigin, aNames[i]));
    }
}

void
GLContext::CreatedRenderbuffers(GLContext* aOrigin, GLsizei aCount, GLuint* aNames)
{
    for (GLsizei i = 0; i < aCount; ++i) {
        mTrackedRenderbuffers.AppendElement(NamedResource(aOrigin, aNames[i]));
    }
}

static void
RemoveNamesFromArray(GLContext* aOrigin, GLsizei aCount, const GLuint* aNames, nsTArray<GLContext::NamedResource>& aArray)
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
GLContext::DeletedProgram(GLContext* aOrigin, GLuint aName)
{
    RemoveNamesFromArray(aOrigin, 1, &aName, mTrackedPrograms);
}

void
GLContext::DeletedShader(GLContext* aOrigin, GLuint aName)
{
    RemoveNamesFromArray(aOrigin, 1, &aName, mTrackedShaders);
}

void
GLContext::DeletedBuffers(GLContext* aOrigin, GLsizei aCount, const GLuint* aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedBuffers);
}

void
GLContext::DeletedQueries(GLContext* aOrigin, GLsizei aCount, const GLuint* aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedQueries);
}

void
GLContext::DeletedTextures(GLContext* aOrigin, GLsizei aCount, const GLuint* aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedTextures);
}

void
GLContext::DeletedFramebuffers(GLContext* aOrigin, GLsizei aCount, const GLuint* aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedFramebuffers);
}

void
GLContext::DeletedRenderbuffers(GLContext* aOrigin, GLsizei aCount, const GLuint* aNames)
{
    RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedRenderbuffers);
}

static void
MarkContextDestroyedInArray(GLContext* aContext, nsTArray<GLContext::NamedResource>& aArray)
{
    for (uint32_t i = 0; i < aArray.Length(); ++i) {
        if (aArray[i].origin == aContext)
            aArray[i].originDeleted = true;
    }
}

void
GLContext::SharedContextDestroyed(GLContext* aChild)
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
ReportArrayContents(const char* title, const nsTArray<GLContext::NamedResource>& aArray)
{
    if (aArray.Length() == 0)
        return;

    printf_stderr("%s:\n", title);

    nsTArray<GLContext::NamedResource> copy(aArray);
    copy.Sort();

    GLContext* lastContext = nullptr;
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
    if (!ShouldSpew())
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
GLContext::IsOffscreenSizeAllowed(const IntSize& aSize) const
{
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
        mBlitHelper.reset(new GLBlitHelper(this));
    }

    return mBlitHelper.get();
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

/*static*/ bool
GLContext::ShouldDumpExts()
{
    return gfxEnv::GlDumpExtensions();
}

bool
DoesStringMatch(const char* aString, const char* aWantedString)
{
    if (!aString || !aWantedString)
        return false;

    const char* occurrence = strstr(aString, aWantedString);

    // aWanted not found
    if (!occurrence)
        return false;

    // aWantedString preceded by alpha character
    if (occurrence != aString && isalpha(*(occurrence-1)))
        return false;

    // aWantedVendor followed by alpha character
    const char* afterOccurrence = occurrence + strlen(aWantedString);
    if (isalpha(*afterOccurrence))
        return false;

    return true;
}

/*static*/ bool
GLContext::ShouldSpew()
{
    return gfxEnv::GlSpew();
}

void
SplitByChar(const nsACString& str, const char delim, std::vector<nsCString>* const out)
{
    uint32_t start = 0;
    while (true) {
        int32_t end = str.FindChar(' ', start);
        if (end == -1)
            break;

        uint32_t len = (uint32_t)end - start;
        nsDependentCSubstring substr(str, start, len);
        out->push_back(nsCString(substr));

        start = end + 1;
        continue;
    }

    nsDependentCSubstring substr(str, start);
    out->push_back(nsCString(substr));
}

void
GLContext::Readback(SharedSurface* src, gfx::DataSourceSurface* dest)
{
    MOZ_ASSERT(src && dest);
    MOZ_ASSERT(dest->GetSize() == src->mSize);
    MOZ_ASSERT(dest->GetFormat() == (src->mHasAlpha ? SurfaceFormat::B8G8R8A8
                                                    : SurfaceFormat::B8G8R8X8));

    MakeCurrent();

    SharedSurface* prev = GetLockedSurface();

    const bool needsSwap = src != prev;
    if (needsSwap) {
        if (prev)
            prev->UnlockProd();
        src->LockProd();
    }

    GLuint tempFB = 0;
    GLuint tempTex = 0;

    {
        ScopedBindFramebuffer autoFB(this);

        // We're consuming from the producer side, so which do we use?
        // Really, we just want a read-only lock, so ConsumerAcquire is the best match.
        src->ProducerReadAcquire();

        if (src->mAttachType == AttachmentType::Screen) {
            fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
        } else {
            fGenFramebuffers(1, &tempFB);
            fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, tempFB);

            switch (src->mAttachType) {
            case AttachmentType::GLTexture:
                fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                      src->ProdTextureTarget(), src->ProdTexture(), 0);
                break;
            case AttachmentType::GLRenderbuffer:
                fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                         LOCAL_GL_RENDERBUFFER, src->ProdRenderbuffer());
                break;
            default:
                MOZ_CRASH("GFX: bad `src->mAttachType`.");
            }

            DebugOnly<GLenum> status = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
            MOZ_ASSERT(status == LOCAL_GL_FRAMEBUFFER_COMPLETE);
        }

        if (src->NeedsIndirectReads()) {
            fGenTextures(1, &tempTex);
            {
                ScopedBindTexture autoTex(this, tempTex);

                GLenum format = src->mHasAlpha ? LOCAL_GL_RGBA
                                               : LOCAL_GL_RGB;
                auto width = src->mSize.width;
                auto height = src->mSize.height;
                fCopyTexImage2D(LOCAL_GL_TEXTURE_2D, 0, format, 0, 0, width,
                                height, 0);
            }

            fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                  LOCAL_GL_COLOR_ATTACHMENT0,
                                  LOCAL_GL_TEXTURE_2D, tempTex, 0);
        }

        ReadPixelsIntoDataSurface(this, dest);

        src->ProducerReadRelease();
    }

    if (tempFB)
        fDeleteFramebuffers(1, &tempFB);

    if (tempTex) {
        fDeleteTextures(1, &tempTex);
    }

    if (needsSwap) {
        src->UnlockProd();
        if (prev)
            prev->LockProd();
    }
}

// Do whatever tear-down is necessary after drawing to our offscreen FBO,
// if it's bound.
void
GLContext::AfterGLDrawCall()
{
    if (mScreen) {
        mScreen->AfterDrawCall();
    }
    mHeavyGLCallsSinceLastFlush = true;
}

// Do whatever setup is necessary to read from our offscreen FBO, if it's
// bound.
void
GLContext::BeforeGLReadCall()
{
    if (mScreen)
        mScreen->BeforeReadCall();
}

void
GLContext::fBindFramebuffer(GLenum target, GLuint framebuffer)
{
    if (!mScreen) {
        raw_fBindFramebuffer(target, framebuffer);
        return;
    }

    switch (target) {
        case LOCAL_GL_DRAW_FRAMEBUFFER_EXT:
            mScreen->BindDrawFB(framebuffer);
            return;

        case LOCAL_GL_READ_FRAMEBUFFER_EXT:
            mScreen->BindReadFB(framebuffer);
            return;

        case LOCAL_GL_FRAMEBUFFER:
            mScreen->BindFB(framebuffer);
            return;

        default:
            // Nothing we care about, likely an error.
            break;
    }

    raw_fBindFramebuffer(target, framebuffer);
}

void
GLContext::fCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                           GLint y, GLsizei width, GLsizei height, GLint border)
{
    if (!IsTextureSizeSafeToPassToDriver(target, width, height)) {
        // pass wrong values to cause the GL to generate GL_INVALID_VALUE.
        // See bug 737182 and the comment in IsTextureSizeSafeToPassToDriver.
        level = -1;
        width = -1;
        height = -1;
        border = -1;
    }

    BeforeGLReadCall();
    bool didCopyTexImage2D = false;
    if (mScreen) {
        didCopyTexImage2D = mScreen->CopyTexImage2D(target, level, internalformat, x,
                                                    y, width, height, border);
    }

    if (!didCopyTexImage2D) {
        raw_fCopyTexImage2D(target, level, internalformat, x, y, width, height,
                            border);
    }
    AfterGLReadCall();
}

void
GLContext::fGetIntegerv(GLenum pname, GLint* params)
{
    switch (pname) {
        // LOCAL_GL_FRAMEBUFFER_BINDING is equal to
        // LOCAL_GL_DRAW_FRAMEBUFFER_BINDING_EXT,
        // so we don't need two cases.
        case LOCAL_GL_DRAW_FRAMEBUFFER_BINDING_EXT:
            if (mScreen) {
                *params = mScreen->GetDrawFB();
            } else {
                raw_fGetIntegerv(pname, params);
            }
            break;

        case LOCAL_GL_READ_FRAMEBUFFER_BINDING_EXT:
            if (mScreen) {
                *params = mScreen->GetReadFB();
            } else {
                raw_fGetIntegerv(pname, params);
            }
            break;

        case LOCAL_GL_MAX_TEXTURE_SIZE:
            MOZ_ASSERT(mMaxTextureSize>0);
            *params = mMaxTextureSize;
            break;

        case LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE:
            MOZ_ASSERT(mMaxCubeMapTextureSize>0);
            *params = mMaxCubeMapTextureSize;
            break;

        case LOCAL_GL_MAX_RENDERBUFFER_SIZE:
            MOZ_ASSERT(mMaxRenderbufferSize>0);
            *params = mMaxRenderbufferSize;
            break;

        case LOCAL_GL_VIEWPORT:
            for (size_t i = 0; i < 4; i++) {
                params[i] = mViewportRect[i];
            }
            break;

        case LOCAL_GL_SCISSOR_BOX:
            for (size_t i = 0; i < 4; i++) {
                params[i] = mScissorRect[i];
            }
            break;

        default:
            raw_fGetIntegerv(pname, params);
            break;
    }
}

void
GLContext::fReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                       GLenum type, GLvoid* pixels)
{
    BeforeGLReadCall();

    bool didReadPixels = false;
    if (mScreen) {
        didReadPixels = mScreen->ReadPixels(x, y, width, height, format, type, pixels);
    }

    if (!didReadPixels) {
        raw_fReadPixels(x, y, width, height, format, type, pixels);
    }

    AfterGLReadCall();
}

void
GLContext::fDeleteFramebuffers(GLsizei n, const GLuint* names)
{
    if (mScreen) {
        // Notify mScreen which framebuffers we're deleting.
        // Otherwise, we will get framebuffer binding mispredictions.
        for (int i = 0; i < n; i++) {
            mScreen->DeletingFB(names[i]);
        }
    }

    // Avoid crash by flushing before glDeleteFramebuffers. See bug 1194923.
    if (mNeedsFlushBeforeDeleteFB) {
        fFlush();
    }

    if (n == 1 && *names == 0) {
        // Deleting framebuffer 0 causes hangs on the DROID. See bug 623228.
    } else {
        raw_fDeleteFramebuffers(n, names);
    }
    TRACKING_CONTEXT(DeletedFramebuffers(this, n, names));
}

#ifdef MOZ_WIDGET_ANDROID
/**
 * Conservatively estimate whether there is enough available
 * contiguous virtual address space to map a newly allocated texture.
 */
static bool
WillTextureMapSucceed(GLsizei width, GLsizei height, GLenum format, GLenum type)
{
    bool willSucceed = false;
    // Some drivers leave large gaps between textures, so require
    // there to be double the actual size of the texture available.
    size_t size = width * height * GetBytesPerTexel(format, type) * 2;

    int fd = open("/dev/zero", O_RDONLY);

    void *p = mmap(nullptr, size, PROT_NONE, MAP_SHARED, fd, 0);
    if (p != MAP_FAILED) {
        willSucceed = true;
        munmap(p, size);
    }

    close(fd);

    return willSucceed;
}
#endif // MOZ_WIDGET_ANDROID

void
GLContext::fTexImage2D(GLenum target, GLint level, GLint internalformat,
                       GLsizei width, GLsizei height, GLint border,
                       GLenum format, GLenum type, const GLvoid* pixels) {
    if (!IsTextureSizeSafeToPassToDriver(target, width, height)) {
        // pass wrong values to cause the GL to generate GL_INVALID_VALUE.
        // See bug 737182 and the comment in IsTextureSizeSafeToPassToDriver.
        level = -1;
        width = -1;
        height = -1;
        border = -1;
    }
#if MOZ_WIDGET_ANDROID
    if (mTextureAllocCrashesOnMapFailure) {
        // We have no way of knowing whether this texture already has
        // storage allocated for it, and therefore whether this check
        // is necessary. We must therefore assume it does not and
        // always perform the check.
        if (!WillTextureMapSucceed(width, height, internalformat, type)) {
            return;
        }
    }
#endif
    raw_fTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

GLuint
GLContext::GetDrawFB()
{
    if (mScreen)
        return mScreen->GetDrawFB();

    GLuint ret = 0;
    GetUIntegerv(LOCAL_GL_DRAW_FRAMEBUFFER_BINDING_EXT, &ret);
    return ret;
}

GLuint
GLContext::GetReadFB()
{
    if (mScreen)
        return mScreen->GetReadFB();

    GLenum bindEnum = IsSupported(GLFeature::split_framebuffer)
                        ? LOCAL_GL_READ_FRAMEBUFFER_BINDING_EXT
                        : LOCAL_GL_FRAMEBUFFER_BINDING;

    GLuint ret = 0;
    GetUIntegerv(bindEnum, &ret);
    return ret;
}

GLuint
GLContext::GetFB()
{
    if (mScreen) {
        // This has a very important extra assert that checks that we're
        // not accidentally ignoring a situation where the draw and read
        // FBs differ.
        return mScreen->GetFB();
    }

    GLuint ret = 0;
    GetUIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &ret);
    return ret;
}

bool
GLContext::InitOffscreen(const gfx::IntSize& size, const SurfaceCaps& caps)
{
    if (!CreateScreenBuffer(size, caps))
        return false;

    MakeCurrent();
    fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
    fScissor(0, 0, size.width, size.height);
    fViewport(0, 0, size.width, size.height);

    mCaps = mScreen->mCaps;
    MOZ_ASSERT(!mCaps.any);

    return true;
}

bool
GLContext::IsDrawingToDefaultFramebuffer()
{
    return Screen()->IsDrawFramebufferDefault();
}

GLuint
CreateTexture(GLContext* aGL, GLenum aInternalFormat, GLenum aFormat,
              GLenum aType, const gfx::IntSize& aSize, bool linear)
{
    GLuint tex = 0;
    aGL->fGenTextures(1, &tex);
    ScopedBindTexture autoTex(aGL, tex);

    aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D,
                        LOCAL_GL_TEXTURE_MIN_FILTER, linear ? LOCAL_GL_LINEAR
                                                            : LOCAL_GL_NEAREST);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D,
                        LOCAL_GL_TEXTURE_MAG_FILTER, linear ? LOCAL_GL_LINEAR
                                                            : LOCAL_GL_NEAREST);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                        LOCAL_GL_CLAMP_TO_EDGE);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                        LOCAL_GL_CLAMP_TO_EDGE);

    aGL->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                     0,
                     aInternalFormat,
                     aSize.width, aSize.height,
                     0,
                     aFormat,
                     aType,
                     nullptr);

    return tex;
}

GLuint
CreateTextureForOffscreen(GLContext* aGL, const GLFormats& aFormats,
                          const gfx::IntSize& aSize)
{
    MOZ_ASSERT(aFormats.color_texInternalFormat);
    MOZ_ASSERT(aFormats.color_texFormat);
    MOZ_ASSERT(aFormats.color_texType);

    GLenum internalFormat = aFormats.color_texInternalFormat;
    GLenum unpackFormat = aFormats.color_texFormat;
    GLenum unpackType = aFormats.color_texType;
    if (aGL->IsANGLE()) {
        MOZ_ASSERT(internalFormat == LOCAL_GL_RGBA);
        MOZ_ASSERT(unpackFormat == LOCAL_GL_RGBA);
        MOZ_ASSERT(unpackType == LOCAL_GL_UNSIGNED_BYTE);
        internalFormat = LOCAL_GL_BGRA_EXT;
        unpackFormat = LOCAL_GL_BGRA_EXT;
    }

    return CreateTexture(aGL, internalFormat, unpackFormat, unpackType, aSize);
}

uint32_t
GetBytesPerTexel(GLenum format, GLenum type)
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

    if (type == LOCAL_GL_UNSIGNED_BYTE || type == LOCAL_GL_FLOAT || type == LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV) {
        uint32_t multiplier = type == LOCAL_GL_UNSIGNED_BYTE ? 1 : 4;
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
            default:
                break;
        }
    } else if (type == LOCAL_GL_UNSIGNED_SHORT_4_4_4_4 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_5_5_1 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_6_5)
    {
        return 2;
    }

    gfxCriticalError() << "Unknown texture type " << type << " or format " << format;
    MOZ_CRASH();
    return 0;
}

} /* namespace gl */
} /* namespace mozilla */
