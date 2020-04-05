/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <regex>
#include <string>
#include <vector>
#ifdef MOZ_WIDGET_ANDROID
#  include <sys/mman.h>
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
#include "gfx2DGlue.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_gl.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/TextureForwarder.h"  // for LayersIPCChannel

#include "OGLShaderProgram.h"  // for ShaderProgramType

#include "mozilla/DebugOnly.h"

#ifdef XP_MACOSX
#  include <CoreServices/CoreServices.h>
#endif

#if defined(MOZ_WIDGET_COCOA)
#  include "nsCocoaFeatures.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/jni/Utils.h"
#endif

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::layers;

MOZ_THREAD_LOCAL(uintptr_t) GLContext::sCurrentContext;

// If adding defines, don't forget to undefine symbols. See #undef block below.
// clang-format off
#define CORE_SYMBOL(x) { (PRFuncPtr*) &mSymbols.f##x, {{ "gl" #x }} }
#define CORE_EXT_SYMBOL2(x,y,z) { (PRFuncPtr*) &mSymbols.f##x, {{ "gl" #x, "gl" #x #y, "gl" #x #z }} }
#define EXT_SYMBOL2(x,y,z) { (PRFuncPtr*) &mSymbols.f##x, {{ "gl" #x #y, "gl" #x #z }} }
#define EXT_SYMBOL3(x,y,z,w) { (PRFuncPtr*) &mSymbols.f##x, {{ "gl" #x #y, "gl" #x #z, "gl" #x #w }} }
#define END_SYMBOLS { nullptr, {} }
// clang-format on

// should match the order of GLExtensions, and be null-terminated.
static const char* const sExtensionNames[] = {
    "NO_EXTENSION",
    "GL_AMD_compressed_ATC_texture",
    "GL_ANGLE_depth_texture",
    "GL_ANGLE_framebuffer_blit",
    "GL_ANGLE_framebuffer_multisample",
    "GL_ANGLE_instanced_arrays",
    "GL_ANGLE_multiview",
    "GL_ANGLE_texture_compression_dxt3",
    "GL_ANGLE_texture_compression_dxt5",
    "GL_ANGLE_timer_query",
    "GL_APPLE_client_storage",
    "GL_APPLE_fence",
    "GL_APPLE_framebuffer_multisample",
    "GL_APPLE_sync",
    "GL_APPLE_texture_range",
    "GL_APPLE_vertex_array_object",
    "GL_ARB_ES2_compatibility",
    "GL_ARB_ES3_compatibility",
    "GL_ARB_color_buffer_float",
    "GL_ARB_compatibility",
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
    "GL_ARB_robust_buffer_access_behavior",
    "GL_ARB_robustness",
    "GL_ARB_sampler_objects",
    "GL_ARB_seamless_cube_map",
    "GL_ARB_shader_texture_lod",
    "GL_ARB_sync",
    "GL_ARB_texture_compression",
    "GL_ARB_texture_compression_bptc",
    "GL_ARB_texture_compression_rgtc",
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
    "GL_CHROMIUM_color_buffer_float_rgb",
    "GL_CHROMIUM_color_buffer_float_rgba",
    "GL_EXT_bgra",
    "GL_EXT_blend_minmax",
    "GL_EXT_color_buffer_float",
    "GL_EXT_color_buffer_half_float",
    "GL_EXT_copy_texture",
    "GL_EXT_disjoint_timer_query",
    "GL_EXT_draw_buffers",
    "GL_EXT_draw_buffers2",
    "GL_EXT_draw_instanced",
    "GL_EXT_float_blend",
    "GL_EXT_frag_depth",
    "GL_EXT_framebuffer_blit",
    "GL_EXT_framebuffer_multisample",
    "GL_EXT_framebuffer_object",
    "GL_EXT_framebuffer_sRGB",
    "GL_EXT_gpu_shader4",
    "GL_EXT_map_buffer_range",
    "GL_EXT_multisampled_render_to_texture",
    "GL_EXT_occlusion_query_boolean",
    "GL_EXT_packed_depth_stencil",
    "GL_EXT_read_format_bgra",
    "GL_EXT_robustness",
    "GL_EXT_sRGB",
    "GL_EXT_sRGB_write_control",
    "GL_EXT_shader_texture_lod",
    "GL_EXT_texture3D",
    "GL_EXT_texture_compression_bptc",
    "GL_EXT_texture_compression_dxt1",
    "GL_EXT_texture_compression_rgtc",
    "GL_EXT_texture_compression_s3tc",
    "GL_EXT_texture_compression_s3tc_srgb",
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
    "GL_KHR_robust_buffer_access_behavior",
    "GL_KHR_robustness",
    "GL_KHR_texture_compression_astc_hdr",
    "GL_KHR_texture_compression_astc_ldr",
    "GL_NV_draw_instanced",
    "GL_NV_fence",
    "GL_NV_framebuffer_blit",
    "GL_NV_geometry_program4",
    "GL_NV_half_float",
    "GL_NV_instanced_arrays",
    "GL_NV_primitive_restart",
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
    "GL_OES_fbo_render_mipmap",
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
    "GL_OES_vertex_array_object",
    "GL_OVR_multiview2"};

static bool ShouldUseTLSIsCurrent(bool useTLSIsCurrent) {
  if (StaticPrefs::gl_use_tls_is_current() == 0) {
    return useTLSIsCurrent;
  }

  return StaticPrefs::gl_use_tls_is_current() > 0;
}

static bool ParseVersion(const std::string& versionStr,
                         uint32_t* const out_major, uint32_t* const out_minor) {
  static const std::regex kVersionRegex("([0-9]+)\\.([0-9]+)");
  std::smatch match;
  if (!std::regex_search(versionStr, match, kVersionRegex)) return false;

  const auto& majorStr = match.str(1);
  const auto& minorStr = match.str(2);
  *out_major = atoi(majorStr.c_str());
  *out_minor = atoi(minorStr.c_str());
  return true;
}

/*static*/
uint8_t GLContext::ChooseDebugFlags(const CreateContextFlags createFlags) {
  uint8_t debugFlags = 0;

#ifdef MOZ_GL_DEBUG
  if (gfxEnv::GlDebug()) {
    debugFlags |= GLContext::DebugFlagEnabled;
  }

  // Enables extra verbose output, informing of the start and finish of every GL
  // call. Useful e.g. to record information to investigate graphics system
  // crashes/lockups
  if (gfxEnv::GlDebugVerbose()) {
    debugFlags |= GLContext::DebugFlagTrace;
  }

  // Aborts on GL error. Can be useful to debug quicker code that is known not
  // to generate any GL error in principle.
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
                     GLContext* sharedContext, bool isOffscreen,
                     bool useTLSIsCurrent)
    : mUseTLSIsCurrent(ShouldUseTLSIsCurrent(useTLSIsCurrent)),
      mIsOffscreen(isOffscreen),
      mDebugFlags(ChooseDebugFlags(flags)),
      mSharedContext(sharedContext),
      mCaps(caps),
      mWorkAroundDriverBugs(
          StaticPrefs::gfx_work_around_driver_bugs_AtStartup()) {
  mOwningThreadId = PlatformThread::CurrentId();
  MOZ_ALWAYS_TRUE(sCurrentContext.init());
  sCurrentContext.set(0);
}

GLContext::~GLContext() {
  NS_ASSERTION(
      IsDestroyed(),
      "GLContext implementation must call MarkDestroyed in destructor!");
#ifdef MOZ_GL_DEBUG
  if (mSharedContext) {
    GLContext* tip = mSharedContext;
    while (tip->mSharedContext) tip = tip->mSharedContext;
    tip->SharedContextDestroyed(this);
    tip->ReportOutstandingNames();
  } else {
    ReportOutstandingNames();
  }
#endif
}

/*static*/
void GLContext::StaticDebugCallback(GLenum source, GLenum type, GLuint id,
                                    GLenum severity, GLsizei length,
                                    const GLchar* message,
                                    const GLvoid* userParam) {
  GLContext* gl = (GLContext*)userParam;
  gl->DebugCallback(source, type, id, severity, length, message);
}

bool GLContext::Init() {
  MOZ_RELEASE_ASSERT(!mSymbols.fBindFramebuffer,
                     "GFX: GLContext::Init should only be called once.");

  ScopedGfxFeatureReporter reporter("GL Context");

  if (!InitImpl()) {
    // If initialization fails, zero the symbols to avoid hard-to-understand
    // bugs.
    mSymbols = {};
    NS_WARNING("GLContext::InitWithPrefix failed!");
    return false;
  }

  reporter.SetSuccessful();
  return true;
}

static bool LoadSymbolsWithDesc(const SymbolLoader& loader,
                                const SymLoadStruct* list, const char* desc) {
  const auto warnOnFailure = bool(desc);
  if (loader.LoadSymbols(list, warnOnFailure)) return true;

  ClearSymbols(list);

  if (desc) {
    const nsPrintfCString err("Failed to load symbols for %s.", desc);
    NS_ERROR(err.BeginReading());
  }
  return false;
}

bool GLContext::LoadExtSymbols(const SymbolLoader& loader,
                               const SymLoadStruct* list, GLExtensions ext) {
  const char* extName = sExtensionNames[size_t(ext)];
  if (!LoadSymbolsWithDesc(loader, list, extName)) {
    MarkExtensionUnsupported(ext);
    return false;
  }
  return true;
};

bool GLContext::LoadFeatureSymbols(const SymbolLoader& loader,
                                   const SymLoadStruct* list,
                                   GLFeature feature) {
  const char* featureName = GetFeatureName(feature);
  if (!LoadSymbolsWithDesc(loader, list, featureName)) {
    MarkUnsupported(feature);
    return false;
  }
  return true;
};

bool GLContext::InitImpl() {
  if (!MakeCurrent(true)) return false;

  const auto loader = GetSymbolLoader();
  if (!loader) return false;

  const auto fnLoadSymbols = [&](const SymLoadStruct* const list,
                                 const char* const desc) {
    return LoadSymbolsWithDesc(*loader, list, desc);
  };

  // clang-format off
    const SymLoadStruct coreSymbols[] = {
        { (PRFuncPtr*) &mSymbols.fActiveTexture, {{ "glActiveTexture", "glActiveTextureARB" }} },
        { (PRFuncPtr*) &mSymbols.fAttachShader, {{ "glAttachShader", "glAttachShaderARB" }} },
        { (PRFuncPtr*) &mSymbols.fBindAttribLocation, {{ "glBindAttribLocation", "glBindAttribLocationARB" }} },
        { (PRFuncPtr*) &mSymbols.fBindBuffer, {{ "glBindBuffer", "glBindBufferARB" }} },
        { (PRFuncPtr*) &mSymbols.fBindTexture, {{ "glBindTexture", "glBindTextureARB" }} },
        { (PRFuncPtr*) &mSymbols.fBlendColor, {{ "glBlendColor" }} },
        { (PRFuncPtr*) &mSymbols.fBlendEquation, {{ "glBlendEquation" }} },
        { (PRFuncPtr*) &mSymbols.fBlendEquationSeparate, {{ "glBlendEquationSeparate", "glBlendEquationSeparateEXT" }} },
        { (PRFuncPtr*) &mSymbols.fBlendFunc, {{ "glBlendFunc" }} },
        { (PRFuncPtr*) &mSymbols.fBlendFuncSeparate, {{ "glBlendFuncSeparate", "glBlendFuncSeparateEXT" }} },
        { (PRFuncPtr*) &mSymbols.fBufferData, {{ "glBufferData" }} },
        { (PRFuncPtr*) &mSymbols.fBufferSubData, {{ "glBufferSubData" }} },
        { (PRFuncPtr*) &mSymbols.fClear, {{ "glClear" }} },
        { (PRFuncPtr*) &mSymbols.fClearColor, {{ "glClearColor" }} },
        { (PRFuncPtr*) &mSymbols.fClearStencil, {{ "glClearStencil" }} },
        { (PRFuncPtr*) &mSymbols.fColorMask, {{ "glColorMask" }} },
        { (PRFuncPtr*) &mSymbols.fCompressedTexImage2D, {{ "glCompressedTexImage2D" }} },
        { (PRFuncPtr*) &mSymbols.fCompressedTexSubImage2D, {{ "glCompressedTexSubImage2D" }} },
        { (PRFuncPtr*) &mSymbols.fCullFace, {{ "glCullFace" }} },
        { (PRFuncPtr*) &mSymbols.fDetachShader, {{ "glDetachShader", "glDetachShaderARB" }} },
        { (PRFuncPtr*) &mSymbols.fDepthFunc, {{ "glDepthFunc" }} },
        { (PRFuncPtr*) &mSymbols.fDepthMask, {{ "glDepthMask" }} },
        { (PRFuncPtr*) &mSymbols.fDisable, {{ "glDisable" }} },
        { (PRFuncPtr*) &mSymbols.fDisableVertexAttribArray, {{ "glDisableVertexAttribArray", "glDisableVertexAttribArrayARB" }} },
        { (PRFuncPtr*) &mSymbols.fDrawArrays, {{ "glDrawArrays" }} },
        { (PRFuncPtr*) &mSymbols.fDrawElements, {{ "glDrawElements" }} },
        { (PRFuncPtr*) &mSymbols.fEnable, {{ "glEnable" }} },
        { (PRFuncPtr*) &mSymbols.fEnableVertexAttribArray, {{ "glEnableVertexAttribArray", "glEnableVertexAttribArrayARB" }} },
        { (PRFuncPtr*) &mSymbols.fFinish, {{ "glFinish" }} },
        { (PRFuncPtr*) &mSymbols.fFlush, {{ "glFlush" }} },
        { (PRFuncPtr*) &mSymbols.fFrontFace, {{ "glFrontFace" }} },
        { (PRFuncPtr*) &mSymbols.fGetActiveAttrib, {{ "glGetActiveAttrib", "glGetActiveAttribARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetActiveUniform, {{ "glGetActiveUniform", "glGetActiveUniformARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetAttachedShaders, {{ "glGetAttachedShaders", "glGetAttachedShadersARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetAttribLocation, {{ "glGetAttribLocation", "glGetAttribLocationARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetIntegerv, {{ "glGetIntegerv" }} },
        { (PRFuncPtr*) &mSymbols.fGetFloatv, {{ "glGetFloatv" }} },
        { (PRFuncPtr*) &mSymbols.fGetBooleanv, {{ "glGetBooleanv" }} },
        { (PRFuncPtr*) &mSymbols.fGetBufferParameteriv, {{ "glGetBufferParameteriv", "glGetBufferParameterivARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetError, {{ "glGetError" }} },
        { (PRFuncPtr*) &mSymbols.fGetProgramiv, {{ "glGetProgramiv", "glGetProgramivARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetProgramInfoLog, {{ "glGetProgramInfoLog", "glGetProgramInfoLogARB" }} },
        { (PRFuncPtr*) &mSymbols.fTexParameteri, {{ "glTexParameteri" }} },
        { (PRFuncPtr*) &mSymbols.fTexParameteriv, {{ "glTexParameteriv" }} },
        { (PRFuncPtr*) &mSymbols.fTexParameterf, {{ "glTexParameterf" }} },
        { (PRFuncPtr*) &mSymbols.fGetString, {{ "glGetString" }} },
        { (PRFuncPtr*) &mSymbols.fGetTexParameterfv, {{ "glGetTexParameterfv" }} },
        { (PRFuncPtr*) &mSymbols.fGetTexParameteriv, {{ "glGetTexParameteriv" }} },
        { (PRFuncPtr*) &mSymbols.fGetUniformfv, {{ "glGetUniformfv", "glGetUniformfvARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetUniformiv, {{ "glGetUniformiv", "glGetUniformivARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetUniformLocation, {{ "glGetUniformLocation", "glGetUniformLocationARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetVertexAttribfv, {{ "glGetVertexAttribfv", "glGetVertexAttribfvARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetVertexAttribiv, {{ "glGetVertexAttribiv", "glGetVertexAttribivARB" }} },
        { (PRFuncPtr*) &mSymbols.fGetVertexAttribPointerv, {{ "glGetVertexAttribPointerv" }} },
        { (PRFuncPtr*) &mSymbols.fHint, {{ "glHint" }} },
        { (PRFuncPtr*) &mSymbols.fIsBuffer, {{ "glIsBuffer", "glIsBufferARB" }} },
        { (PRFuncPtr*) &mSymbols.fIsEnabled, {{ "glIsEnabled" }} },
        { (PRFuncPtr*) &mSymbols.fIsProgram, {{ "glIsProgram", "glIsProgramARB" }} },
        { (PRFuncPtr*) &mSymbols.fIsShader, {{ "glIsShader", "glIsShaderARB" }} },
        { (PRFuncPtr*) &mSymbols.fIsTexture, {{ "glIsTexture", "glIsTextureARB" }} },
        { (PRFuncPtr*) &mSymbols.fLineWidth, {{ "glLineWidth" }} },
        { (PRFuncPtr*) &mSymbols.fLinkProgram, {{ "glLinkProgram", "glLinkProgramARB" }} },
        { (PRFuncPtr*) &mSymbols.fPixelStorei, {{ "glPixelStorei" }} },
        { (PRFuncPtr*) &mSymbols.fPolygonOffset, {{ "glPolygonOffset" }} },
        { (PRFuncPtr*) &mSymbols.fReadPixels, {{ "glReadPixels" }} },
        { (PRFuncPtr*) &mSymbols.fSampleCoverage, {{ "glSampleCoverage" }} },
        { (PRFuncPtr*) &mSymbols.fScissor, {{ "glScissor" }} },
        { (PRFuncPtr*) &mSymbols.fStencilFunc, {{ "glStencilFunc" }} },
        { (PRFuncPtr*) &mSymbols.fStencilFuncSeparate, {{ "glStencilFuncSeparate", "glStencilFuncSeparateEXT" }} },
        { (PRFuncPtr*) &mSymbols.fStencilMask, {{ "glStencilMask" }} },
        { (PRFuncPtr*) &mSymbols.fStencilMaskSeparate, {{ "glStencilMaskSeparate", "glStencilMaskSeparateEXT" }} },
        { (PRFuncPtr*) &mSymbols.fStencilOp, {{ "glStencilOp" }} },
        { (PRFuncPtr*) &mSymbols.fStencilOpSeparate, {{ "glStencilOpSeparate", "glStencilOpSeparateEXT" }} },
        { (PRFuncPtr*) &mSymbols.fTexImage2D, {{ "glTexImage2D" }} },
        { (PRFuncPtr*) &mSymbols.fTexSubImage2D, {{ "glTexSubImage2D" }} },
        { (PRFuncPtr*) &mSymbols.fUniform1f, {{ "glUniform1f" }} },
        { (PRFuncPtr*) &mSymbols.fUniform1fv, {{ "glUniform1fv" }} },
        { (PRFuncPtr*) &mSymbols.fUniform1i, {{ "glUniform1i" }} },
        { (PRFuncPtr*) &mSymbols.fUniform1iv, {{ "glUniform1iv" }} },
        { (PRFuncPtr*) &mSymbols.fUniform2f, {{ "glUniform2f" }} },
        { (PRFuncPtr*) &mSymbols.fUniform2fv, {{ "glUniform2fv" }} },
        { (PRFuncPtr*) &mSymbols.fUniform2i, {{ "glUniform2i" }} },
        { (PRFuncPtr*) &mSymbols.fUniform2iv, {{ "glUniform2iv" }} },
        { (PRFuncPtr*) &mSymbols.fUniform3f, {{ "glUniform3f" }} },
        { (PRFuncPtr*) &mSymbols.fUniform3fv, {{ "glUniform3fv" }} },
        { (PRFuncPtr*) &mSymbols.fUniform3i, {{ "glUniform3i" }} },
        { (PRFuncPtr*) &mSymbols.fUniform3iv, {{ "glUniform3iv" }} },
        { (PRFuncPtr*) &mSymbols.fUniform4f, {{ "glUniform4f" }} },
        { (PRFuncPtr*) &mSymbols.fUniform4fv, {{ "glUniform4fv" }} },
        { (PRFuncPtr*) &mSymbols.fUniform4i, {{ "glUniform4i" }} },
        { (PRFuncPtr*) &mSymbols.fUniform4iv, {{ "glUniform4iv" }} },
        { (PRFuncPtr*) &mSymbols.fUniformMatrix2fv, {{ "glUniformMatrix2fv" }} },
        { (PRFuncPtr*) &mSymbols.fUniformMatrix3fv, {{ "glUniformMatrix3fv" }} },
        { (PRFuncPtr*) &mSymbols.fUniformMatrix4fv, {{ "glUniformMatrix4fv" }} },
        { (PRFuncPtr*) &mSymbols.fUseProgram, {{ "glUseProgram" }} },
        { (PRFuncPtr*) &mSymbols.fValidateProgram, {{ "glValidateProgram" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttribPointer, {{ "glVertexAttribPointer" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib1f, {{ "glVertexAttrib1f" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib2f, {{ "glVertexAttrib2f" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib3f, {{ "glVertexAttrib3f" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib4f, {{ "glVertexAttrib4f" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib1fv, {{ "glVertexAttrib1fv" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib2fv, {{ "glVertexAttrib2fv" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib3fv, {{ "glVertexAttrib3fv" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttrib4fv, {{ "glVertexAttrib4fv" }} },
        { (PRFuncPtr*) &mSymbols.fViewport, {{ "glViewport" }} },
        { (PRFuncPtr*) &mSymbols.fCompileShader, {{ "glCompileShader" }} },
        { (PRFuncPtr*) &mSymbols.fCopyTexImage2D, {{ "glCopyTexImage2D" }} },
        { (PRFuncPtr*) &mSymbols.fCopyTexSubImage2D, {{ "glCopyTexSubImage2D" }} },
        { (PRFuncPtr*) &mSymbols.fGetShaderiv, {{ "glGetShaderiv" }} },
        { (PRFuncPtr*) &mSymbols.fGetShaderInfoLog, {{ "glGetShaderInfoLog" }} },
        { (PRFuncPtr*) &mSymbols.fGetShaderSource, {{ "glGetShaderSource" }} },
        { (PRFuncPtr*) &mSymbols.fShaderSource, {{ "glShaderSource" }} },
        { (PRFuncPtr*) &mSymbols.fVertexAttribPointer, {{ "glVertexAttribPointer" }} },

        { (PRFuncPtr*) &mSymbols.fGenBuffers, {{ "glGenBuffers", "glGenBuffersARB" }} },
        { (PRFuncPtr*) &mSymbols.fGenTextures, {{ "glGenTextures" }} },
        { (PRFuncPtr*) &mSymbols.fCreateProgram, {{ "glCreateProgram", "glCreateProgramARB" }} },
        { (PRFuncPtr*) &mSymbols.fCreateShader, {{ "glCreateShader", "glCreateShaderARB" }} },

        { (PRFuncPtr*) &mSymbols.fDeleteBuffers, {{ "glDeleteBuffers", "glDeleteBuffersARB" }} },
        { (PRFuncPtr*) &mSymbols.fDeleteTextures, {{ "glDeleteTextures", "glDeleteTexturesARB" }} },
        { (PRFuncPtr*) &mSymbols.fDeleteProgram, {{ "glDeleteProgram", "glDeleteProgramARB" }} },
        { (PRFuncPtr*) &mSymbols.fDeleteShader, {{ "glDeleteShader", "glDeleteShaderARB" }} },

        END_SYMBOLS
    };
  // clang-format on

  if (!fnLoadSymbols(coreSymbols, "GL")) return false;

  {
    const SymLoadStruct symbols[] = {
        {(PRFuncPtr*)&mSymbols.fGetGraphicsResetStatus,
         {{"glGetGraphicsResetStatus", "glGetGraphicsResetStatusARB",
           "glGetGraphicsResetStatusKHR", "glGetGraphicsResetStatusEXT"}}},
        END_SYMBOLS};
    (void)fnLoadSymbols(symbols, nullptr);

    auto err = fGetError();
    if (err == LOCAL_GL_CONTEXT_LOST) {
      MOZ_ASSERT(mSymbols.fGetGraphicsResetStatus);
      const auto status = fGetGraphicsResetStatus();
      if (status) {
        printf_stderr("Unflushed glGetGraphicsResetStatus: 0x%04x\n", status);
      }
      err = fGetError();
      MOZ_ASSERT(!err);
    }
    if (err) {
      MOZ_ASSERT(false);
      return false;
    }
  }

  ////////////////

  const std::string versionStr = (const char*)fGetString(LOCAL_GL_VERSION);
  if (versionStr.find("OpenGL ES") == 0) {
    mProfile = ContextProfile::OpenGLES;
  }

  uint32_t majorVer, minorVer;
  if (!ParseVersion(versionStr, &majorVer, &minorVer)) {
    MOZ_ASSERT(false, "Failed to parse GL_VERSION");
    return false;
  }
  MOZ_ASSERT(majorVer < 10);
  MOZ_ASSERT(minorVer < 10);
  mVersion = majorVer * 100 + minorVer * 10;
  if (mVersion < 200) return false;

  ////

  const auto glslVersionStr =
      (const char*)fGetString(LOCAL_GL_SHADING_LANGUAGE_VERSION);
  if (!glslVersionStr) {
    // This happens on the Android emulators. We'll just return 100
    mShadingLanguageVersion = 100;
  } else if (ParseVersion(glslVersionStr, &majorVer, &minorVer)) {
    MOZ_ASSERT(majorVer < 10);
    MOZ_ASSERT(minorVer < 100);
    mShadingLanguageVersion = majorVer * 100 + minorVer;
  } else {
    MOZ_ASSERT(false, "Failed to parse GL_SHADING_LANGUAGE_VERSION");
    return false;
  }

  if (ShouldSpew()) {
    printf_stderr("GL version detected: %u\n", mVersion);
    printf_stderr("GLSL version detected: %u\n", mShadingLanguageVersion);
    printf_stderr("OpenGL vendor: %s\n", fGetString(LOCAL_GL_VENDOR));
    printf_stderr("OpenGL renderer: %s\n", fGetString(LOCAL_GL_RENDERER));
  }

  ////////////////

  // Load OpenGL ES 2.0 symbols, or desktop if we aren't using ES 2.
  if (mProfile == ContextProfile::OpenGLES) {
    const SymLoadStruct symbols[] = {CORE_SYMBOL(GetShaderPrecisionFormat),
                                     CORE_SYMBOL(ClearDepthf),
                                     CORE_SYMBOL(DepthRangef), END_SYMBOLS};

    if (!fnLoadSymbols(symbols, "OpenGL ES")) return false;
  } else {
    const SymLoadStruct symbols[] = {
        CORE_SYMBOL(ClearDepth), CORE_SYMBOL(DepthRange),
        CORE_SYMBOL(ReadBuffer), CORE_SYMBOL(MapBuffer),
        CORE_SYMBOL(UnmapBuffer), CORE_SYMBOL(PointParameterf),
        CORE_SYMBOL(DrawBuffer),
        // The following functions are only used by Skia/GL in desktop mode.
        // Other parts of Gecko should avoid using these
        CORE_SYMBOL(DrawBuffers), CORE_SYMBOL(ClientActiveTexture),
        CORE_SYMBOL(DisableClientState), CORE_SYMBOL(EnableClientState),
        CORE_SYMBOL(LoadIdentity), CORE_SYMBOL(LoadMatrixf),
        CORE_SYMBOL(MatrixMode), CORE_SYMBOL(PolygonMode), CORE_SYMBOL(TexGeni),
        CORE_SYMBOL(TexGenf), CORE_SYMBOL(TexGenfv), CORE_SYMBOL(VertexPointer),
        END_SYMBOLS};

    if (!fnLoadSymbols(symbols, "Desktop OpenGL")) return false;
  }

  ////////////////

  const char* glVendorString = (const char*)fGetString(LOCAL_GL_VENDOR);
  const char* glRendererString = (const char*)fGetString(LOCAL_GL_RENDERER);
  if (!glVendorString || !glRendererString) return false;

  // The order of these strings must match up with the order of the enum
  // defined in GLContext.h for vendor IDs.
  const char* vendorMatchStrings[size_t(GLVendor::Other) + 1] = {
      "Intel",   "NVIDIA",  "ATI",          "Qualcomm", "Imagination",
      "nouveau", "Vivante", "VMware, Inc.", "ARM",      "Unknown"};

  mVendor = GLVendor::Other;
  for (size_t i = 0; i < size_t(GLVendor::Other); ++i) {
    if (DoesStringMatch(glVendorString, vendorMatchStrings[i])) {
      mVendor = GLVendor(i);
      break;
    }
  }

  // The order of these strings must match up with the order of the enum
  // defined in GLContext.h for renderer IDs.
  const char* rendererMatchStrings[size_t(GLRenderer::Other) + 1] = {
      "Adreno 200",
      "Adreno 205",
      "Adreno (TM) 200",
      "Adreno (TM) 205",
      "Adreno (TM) 305",
      "Adreno (TM) 320",
      "Adreno (TM) 330",
      "Adreno (TM) 420",
      "Mali-400 MP",
      "Mali-450 MP",
      "PowerVR SGX 530",
      "PowerVR SGX 540",
      "PowerVR SGX 544MP",
      "NVIDIA Tegra",
      "Android Emulator",
      "Gallium 0.4 on llvmpipe",
      "Intel HD Graphics 3000 OpenGL Engine",
      "Microsoft Basic Render Driver",
      "Unknown"};

  mRenderer = GLRenderer::Other;
  for (size_t i = 0; i < size_t(GLRenderer::Other); ++i) {
    if (DoesStringMatch(glRendererString, rendererMatchStrings[i])) {
      mRenderer = GLRenderer(i);
      break;
    }
  }

  if (ShouldSpew()) {
    printf_stderr("GL_VENDOR: %s\n", glVendorString);
    printf_stderr("mVendor: %s\n", vendorMatchStrings[size_t(mVendor)]);
    printf_stderr("GL_RENDERER: %s\n", glRendererString);
    printf_stderr("mRenderer: %s\n", rendererMatchStrings[size_t(mRenderer)]);
  }

  ////////////////

  if (mVersion >= 300) {  // Both GL3 and ES3.
    const SymLoadStruct symbols[] = {
        {(PRFuncPtr*)&mSymbols.fGetStringi, {{"glGetStringi"}}}, END_SYMBOLS};

    if (!fnLoadSymbols(symbols, "GetStringi")) {
      MOZ_RELEASE_ASSERT(false, "GFX: GetStringi is required!");
      return false;
    }
  }

  InitExtensions();
  if (mProfile != ContextProfile::OpenGLES) {
    if (mVersion >= 310 && !IsExtensionSupported(ARB_compatibility)) {
      mProfile = ContextProfile::OpenGLCore;
    } else {
      mProfile = ContextProfile::OpenGLCompatibility;
    }
  }
  MOZ_ASSERT(mProfile != ContextProfile::Unknown);

  if (ShouldSpew()) {
    const char* profileStr = "";
    if (mProfile == ContextProfile::OpenGLES) {
      profileStr = " es";
    } else if (mProfile == ContextProfile::OpenGLCore) {
      profileStr = " core";
    }
    printf_stderr("Detected profile: %u%s\n", mVersion, profileStr);
  }

  InitFeatures();

  ////

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
      // multisampling hardcodes blending with the default blendfunc, which
      // breaks WebGL.
      MarkUnsupported(GLFeature::framebuffer_multisample);
    }

#ifdef XP_MACOSX
    // The Mac Nvidia driver, for versions up to and including 10.8,
    // don't seem to properly support this.  See 814839
    // this has been fixed in Mac OS X 10.9. See 907946
    // and it also works in 10.8.3 and higher.  See 1094338.
    if (Vendor() == gl::GLVendor::NVIDIA &&
        !nsCocoaFeatures::IsAtLeastVersion(10, 8, 3)) {
      MarkUnsupported(GLFeature::depth_texture);
    }
#endif

    const auto versionStr = (const char*)fGetString(LOCAL_GL_VERSION);
    if (strstr(versionStr, "Mesa")) {
      // DrawElementsInstanced hangs the driver.
      MarkUnsupported(GLFeature::robust_buffer_access_behavior);
    }
  }

  if (IsExtensionSupported(GLContext::ARB_pixel_buffer_object)) {
    MOZ_ASSERT(
        (mSymbols.fMapBuffer && mSymbols.fUnmapBuffer),
        "ARB_pixel_buffer_object supported without glMapBuffer/UnmapBuffer"
        " being available!");
  }

  ////////////////////////////////////////////////////////////////////////////

  const auto fnLoadForFeature = [&](const SymLoadStruct* list,
                                    GLFeature feature) {
    return this->LoadFeatureSymbols(*loader, list, feature);
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
        END_SYMBOLS};
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
          END_SYMBOLS};
      fnLoadForFeature(symbols, GLFeature::framebuffer_object_EXT_OES);
    }

    if (IsSupported(GLFeature::framebuffer_blit)) {
      const SymLoadStruct symbols[] = {
          EXT_SYMBOL3(BlitFramebuffer, ANGLE, EXT, NV), END_SYMBOLS};
      fnLoadForFeature(symbols, GLFeature::framebuffer_blit);
    }

    if (IsSupported(GLFeature::framebuffer_multisample)) {
      const SymLoadStruct symbols[] = {
          EXT_SYMBOL3(RenderbufferStorageMultisample, ANGLE, APPLE, EXT),
          END_SYMBOLS};
      fnLoadForFeature(symbols, GLFeature::framebuffer_multisample);
    }

    if (IsExtensionSupported(GLContext::ARB_geometry_shader4) ||
        IsExtensionSupported(GLContext::NV_geometry_program4)) {
      const SymLoadStruct symbols[] = {
          EXT_SYMBOL2(FramebufferTextureLayer, ARB, EXT), END_SYMBOLS};
      if (!fnLoadSymbols(symbols,
                         "ARB_geometry_shader4/NV_geometry_program4")) {
        MarkExtensionUnsupported(GLContext::ARB_geometry_shader4);
        MarkExtensionUnsupported(GLContext::NV_geometry_program4);
      }
    }
  }

  if (!IsSupported(GLFeature::framebuffer_object) &&
      !IsSupported(GLFeature::framebuffer_object_EXT_OES)) {
    NS_ERROR("GLContext requires support for framebuffer objects.");
    return false;
  }
  MOZ_RELEASE_ASSERT(mSymbols.fBindFramebuffer,
                     "GFX: mSymbols.fBindFramebuffer zero or not set.");

  ////////////////

  const auto err = fGetError();
  MOZ_RELEASE_ASSERT(!IsBadCallError(err));
  if (err) return false;

  LoadMoreSymbols(*loader);

  ////////////////////////////////////////////////////////////////////////////

  raw_fGetIntegerv(LOCAL_GL_VIEWPORT, mViewportRect);
  raw_fGetIntegerv(LOCAL_GL_SCISSOR_BOX, mScissorRect);
  raw_fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
  raw_fGetIntegerv(LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE, &mMaxCubeMapTextureSize);
  raw_fGetIntegerv(LOCAL_GL_MAX_RENDERBUFFER_SIZE, &mMaxRenderbufferSize);
  raw_fGetIntegerv(LOCAL_GL_MAX_VIEWPORT_DIMS, mMaxViewportDims);

  if (mWorkAroundDriverBugs) {
    int maxTexSize = INT32_MAX;
    int maxCubeSize = INT32_MAX;
#ifdef XP_MACOSX
    if (!nsCocoaFeatures::IsAtLeastVersion(10, 12)) {
      if (mVendor == GLVendor::Intel) {
        // see bug 737182 for 2D textures, bug 684882 for cube map textures.
        maxTexSize = 4096;
        maxCubeSize = 512;
      } else if (mVendor == GLVendor::NVIDIA) {
        // See bug 879656.  8192 fails, 8191 works.
        maxTexSize = 8191;
      }
    } else {
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1544446
      // Mojave exposes 16k textures, but gives FRAMEBUFFER_UNSUPPORTED for any
      // 16k*16k FB except rgba8 without depth/stencil.
      // The max supported sizes changes based on involved formats.
      // (RGBA32F more restrictive than RGBA16F)
      maxTexSize = 8192;
    }
#endif
#ifdef MOZ_X11
    if (mVendor == GLVendor::Nouveau) {
      // see bug 814716. Clamp MaxCubeMapTextureSize at 2K for Nouveau.
      maxCubeSize = 2048;
    } else if (mVendor == GLVendor::Intel) {
      // Bug 1199923. Driver seems to report a larger max size than
      // actually supported.
      maxTexSize = mMaxTextureSize / 2;
    }
    // Bug 1367570. Explicitly set vertex attributes [1,3] to opaque
    // black because Nvidia doesn't do it for us.
    if (mVendor == GLVendor::NVIDIA) {
      for (size_t i = 1; i <= 3; ++i) {
        mSymbols.fVertexAttrib4f(i, 0, 0, 0, 1);
      }
    }
#endif
    if (Renderer() == GLRenderer::AdrenoTM420) {
      // see bug 1194923. Calling glFlush before glDeleteFramebuffers
      // prevents occasional driver crash.
      mNeedsFlushBeforeDeleteFB = true;
    }
#ifdef MOZ_WIDGET_ANDROID
    if ((Renderer() == GLRenderer::AdrenoTM305 ||
         Renderer() == GLRenderer::AdrenoTM320 ||
         Renderer() == GLRenderer::AdrenoTM330) &&
        jni::GetAPIVersion() < 21) {
      // Bug 1164027. Driver crashes when functions such as
      // glTexImage2D fail due to virtual memory exhaustion.
      mTextureAllocCrashesOnMapFailure = true;
    }
#endif
#if MOZ_WIDGET_ANDROID
    if (Renderer() == GLRenderer::SGX540 && jni::GetAPIVersion() <= 15) {
      // Bug 1288446. Driver sometimes crashes when uploading data to a
      // texture if the render target has changed since the texture was
      // rendered from. Calling glCheckFramebufferStatus after
      // glFramebufferTexture2D prevents the crash.
      mNeedsCheckAfterAttachTextureToFb = true;
    }
#endif

    // -

    const auto fnLimit = [&](int* const driver, const int limit) {
      if (*driver > limit) {
        *driver = limit;
        mNeedsTextureSizeChecks = true;
      }
    };

    fnLimit(&mMaxTextureSize, maxTexSize);
    fnLimit(&mMaxRenderbufferSize, maxTexSize);

    maxCubeSize = std::min(maxCubeSize, maxTexSize);
    fnLimit(&mMaxCubeMapTextureSize, maxCubeSize);
  }

  if (IsSupported(GLFeature::framebuffer_multisample)) {
    fGetIntegerv(LOCAL_GL_MAX_SAMPLES, (GLint*)&mMaxSamples);
  }

  mMaxTexOrRbSize = std::min(mMaxTextureSize, mMaxRenderbufferSize);

  ////////////////////////////////////////////////////////////////////////////

  // We're ready for final setup.
  fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  // TODO: Remove SurfaceCaps::any.
  if (mCaps.any) {
    mCaps.any = false;
    mCaps.color = true;
    mCaps.alpha = false;
  }

  MOZ_GL_ASSERT(this, IsCurrent());

  if (ShouldSpew() && IsExtensionSupported(KHR_debug)) {
    fEnable(LOCAL_GL_DEBUG_OUTPUT);
    fDisable(LOCAL_GL_DEBUG_OUTPUT_SYNCHRONOUS);
    fDebugMessageCallback(&StaticDebugCallback, (void*)this);
    fDebugMessageControl(LOCAL_GL_DONT_CARE, LOCAL_GL_DONT_CARE,
                         LOCAL_GL_DONT_CARE, 0, nullptr, true);
  }

  return true;
}

void GLContext::LoadMoreSymbols(const SymbolLoader& loader) {
  const auto fnLoadForExt = [&](const SymLoadStruct* list, GLExtensions ext) {
    return this->LoadExtSymbols(loader, list, ext);
  };

  const auto fnLoadForFeature = [&](const SymLoadStruct* list,
                                    GLFeature feature) {
    return this->LoadFeatureSymbols(loader, list, feature);
  };

  const auto fnLoadFeatureByCore = [&](const SymLoadStruct* coreList,
                                       const SymLoadStruct* extList,
                                       GLFeature feature) {
    const bool useCore = this->IsFeatureProvidedByCoreSymbols(feature);
    const auto list = useCore ? coreList : extList;
    return fnLoadForFeature(list, feature);
  };

  if (IsSupported(GLFeature::robustness)) {
    const auto resetStrategy =
        GetIntAs<GLuint>(LOCAL_GL_RESET_NOTIFICATION_STRATEGY);
    if (resetStrategy != LOCAL_GL_LOSE_CONTEXT_ON_RESET) {
      NS_WARNING(
          "Robustness supported, strategy is not LOSE_CONTEXT_ON_RESET!");
      if (ShouldSpew()) {
        const bool isDisabled =
            (resetStrategy == LOCAL_GL_NO_RESET_NOTIFICATION);
        printf_stderr("Strategy: %s (0x%04x)",
                      (isDisabled ? "disabled" : "unrecognized"),
                      resetStrategy);
      }
      MarkUnsupported(GLFeature::robustness);
    }
  }

  if (IsSupported(GLFeature::sync)) {
    const SymLoadStruct symbols[] = {
        CORE_SYMBOL(FenceSync),  CORE_SYMBOL(IsSync),
        CORE_SYMBOL(DeleteSync), CORE_SYMBOL(ClientWaitSync),
        CORE_SYMBOL(WaitSync),   CORE_SYMBOL(GetInteger64v),
        CORE_SYMBOL(GetSynciv),  END_SYMBOLS};
    fnLoadForFeature(symbols, GLFeature::sync);
  }

  if (IsExtensionSupported(OES_EGL_image)) {
    const SymLoadStruct symbols[] = {
        {(PRFuncPtr*)&mSymbols.fEGLImageTargetTexture2D,
         {{"glEGLImageTargetTexture2DOES"}}},
        {(PRFuncPtr*)&mSymbols.fEGLImageTargetRenderbufferStorage,
         {{"glEGLImageTargetRenderbufferStorageOES"}}},
        END_SYMBOLS};
    fnLoadForExt(symbols, OES_EGL_image);
  }

  if (IsExtensionSupported(APPLE_texture_range)) {
    const SymLoadStruct symbols[] = {CORE_SYMBOL(TextureRangeAPPLE),
                                     END_SYMBOLS};
    fnLoadForExt(symbols, APPLE_texture_range);
  }

  if (IsExtensionSupported(APPLE_fence)) {
    const SymLoadStruct symbols[] = {CORE_SYMBOL(FinishObjectAPPLE),
                                     CORE_SYMBOL(TestObjectAPPLE), END_SYMBOLS};
    fnLoadForExt(symbols, APPLE_fence);
  }

  // clang-format off

    if (IsSupported(GLFeature::vertex_array_object)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fIsVertexArray, {{ "glIsVertexArray" }} },
            { (PRFuncPtr*) &mSymbols.fGenVertexArrays, {{ "glGenVertexArrays" }} },
            { (PRFuncPtr*) &mSymbols.fBindVertexArray, {{ "glBindVertexArray" }} },
            { (PRFuncPtr*) &mSymbols.fDeleteVertexArrays, {{ "glDeleteVertexArrays" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fIsVertexArray, {{ "glIsVertexArrayARB", "glIsVertexArrayOES", "glIsVertexArrayAPPLE" }} },
            { (PRFuncPtr*) &mSymbols.fGenVertexArrays, {{ "glGenVertexArraysARB", "glGenVertexArraysOES", "glGenVertexArraysAPPLE" }} },
            { (PRFuncPtr*) &mSymbols.fBindVertexArray, {{ "glBindVertexArrayARB", "glBindVertexArrayOES", "glBindVertexArrayAPPLE" }} },
            { (PRFuncPtr*) &mSymbols.fDeleteVertexArrays, {{ "glDeleteVertexArraysARB", "glDeleteVertexArraysOES", "glDeleteVertexArraysAPPLE" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::vertex_array_object);
    }

    if (IsSupported(GLFeature::draw_instanced)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawArraysInstanced, {{ "glDrawArraysInstanced" }} },
            { (PRFuncPtr*) &mSymbols.fDrawElementsInstanced, {{ "glDrawElementsInstanced" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawArraysInstanced, {{ "glDrawArraysInstancedARB", "glDrawArraysInstancedEXT", "glDrawArraysInstancedNV", "glDrawArraysInstancedANGLE" }} },
            { (PRFuncPtr*) &mSymbols.fDrawElementsInstanced, {{ "glDrawElementsInstancedARB", "glDrawElementsInstancedEXT", "glDrawElementsInstancedNV", "glDrawElementsInstancedANGLE" }}
            },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::draw_instanced);
    }

    if (IsSupported(GLFeature::instanced_arrays)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fVertexAttribDivisor, {{ "glVertexAttribDivisor" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fVertexAttribDivisor, {{ "glVertexAttribDivisorARB", "glVertexAttribDivisorNV", "glVertexAttribDivisorANGLE" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::instanced_arrays);
    }

    if (IsSupported(GLFeature::texture_storage)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fTexStorage2D, {{ "glTexStorage2D" }} },
            { (PRFuncPtr*) &mSymbols.fTexStorage3D, {{ "glTexStorage3D" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fTexStorage2D, {{ "glTexStorage2DEXT" }} },
            { (PRFuncPtr*) &mSymbols.fTexStorage3D, {{ "glTexStorage3DEXT" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_storage);
    }

    if (IsSupported(GLFeature::sampler_objects)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGenSamplers, {{ "glGenSamplers" }} },
            { (PRFuncPtr*) &mSymbols.fDeleteSamplers, {{ "glDeleteSamplers" }} },
            { (PRFuncPtr*) &mSymbols.fIsSampler, {{ "glIsSampler" }} },
            { (PRFuncPtr*) &mSymbols.fBindSampler, {{ "glBindSampler" }} },
            { (PRFuncPtr*) &mSymbols.fSamplerParameteri, {{ "glSamplerParameteri" }} },
            { (PRFuncPtr*) &mSymbols.fSamplerParameteriv, {{ "glSamplerParameteriv" }} },
            { (PRFuncPtr*) &mSymbols.fSamplerParameterf, {{ "glSamplerParameterf" }} },
            { (PRFuncPtr*) &mSymbols.fSamplerParameterfv, {{ "glSamplerParameterfv" }} },
            { (PRFuncPtr*) &mSymbols.fGetSamplerParameteriv, {{ "glGetSamplerParameteriv" }} },
            { (PRFuncPtr*) &mSymbols.fGetSamplerParameterfv, {{ "glGetSamplerParameterfv" }} },
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
            { (PRFuncPtr*) &mSymbols.fBindBufferBase, {{ "glBindBufferBase" }} },
            { (PRFuncPtr*) &mSymbols.fBindBufferRange, {{ "glBindBufferRange" }} },
            { (PRFuncPtr*) &mSymbols.fGenTransformFeedbacks, {{ "glGenTransformFeedbacks" }} },
            { (PRFuncPtr*) &mSymbols.fBindTransformFeedback, {{ "glBindTransformFeedback" }} },
            { (PRFuncPtr*) &mSymbols.fDeleteTransformFeedbacks, {{ "glDeleteTransformFeedbacks" }} },
            { (PRFuncPtr*) &mSymbols.fIsTransformFeedback, {{ "glIsTransformFeedback" }} },
            { (PRFuncPtr*) &mSymbols.fBeginTransformFeedback, {{ "glBeginTransformFeedback" }} },
            { (PRFuncPtr*) &mSymbols.fEndTransformFeedback, {{ "glEndTransformFeedback" }} },
            { (PRFuncPtr*) &mSymbols.fTransformFeedbackVaryings, {{ "glTransformFeedbackVaryings" }} },
            { (PRFuncPtr*) &mSymbols.fGetTransformFeedbackVarying, {{ "glGetTransformFeedbackVarying" }} },
            { (PRFuncPtr*) &mSymbols.fPauseTransformFeedback, {{ "glPauseTransformFeedback" }} },
            { (PRFuncPtr*) &mSymbols.fResumeTransformFeedback, {{ "glResumeTransformFeedback" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBindBufferBase, {{ "glBindBufferBaseEXT", "glBindBufferBaseNV" }} },
            { (PRFuncPtr*) &mSymbols.fBindBufferRange, {{ "glBindBufferRangeEXT", "glBindBufferRangeNV" }} },
            { (PRFuncPtr*) &mSymbols.fGenTransformFeedbacks, {{ "glGenTransformFeedbacksNV" }} },
            { (PRFuncPtr*) &mSymbols.fBindTransformFeedback, {{ "glBindTransformFeedbackNV" }} },
            { (PRFuncPtr*) &mSymbols.fDeleteTransformFeedbacks, {{ "glDeleteTransformFeedbacksNV" }} },
            { (PRFuncPtr*) &mSymbols.fIsTransformFeedback, {{ "glIsTransformFeedbackNV" }} },
            { (PRFuncPtr*) &mSymbols.fBeginTransformFeedback, {{ "glBeginTransformFeedbackEXT", "glBeginTransformFeedbackNV" }} },
            { (PRFuncPtr*) &mSymbols.fEndTransformFeedback, {{ "glEndTransformFeedbackEXT", "glEndTransformFeedbackNV" }} },
            { (PRFuncPtr*) &mSymbols.fTransformFeedbackVaryings, {{ "glTransformFeedbackVaryingsEXT", "glTransformFeedbackVaryingsNV" }} },
            { (PRFuncPtr*) &mSymbols.fGetTransformFeedbackVarying, {{ "glGetTransformFeedbackVaryingEXT", "glGetTransformFeedbackVaryingNV" }} },
            { (PRFuncPtr*) &mSymbols.fPauseTransformFeedback, {{ "glPauseTransformFeedbackNV" }} },
            { (PRFuncPtr*) &mSymbols.fResumeTransformFeedback, {{ "glResumeTransformFeedbackNV" }} },
            END_SYMBOLS
        };
        if (!fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_storage)) {
            // Also mark bind_buffer_offset as unsupported.
            MarkUnsupported(GLFeature::bind_buffer_offset);
        }
    }

    if (IsSupported(GLFeature::bind_buffer_offset)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBindBufferOffset, {{ "glBindBufferOffset" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBindBufferOffset,
              {{ "glBindBufferOffsetEXT", "glBindBufferOffsetNV" }}
            },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::bind_buffer_offset);
    }

    if (IsSupported(GLFeature::query_counter)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fQueryCounter, {{ "glQueryCounter" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fQueryCounter, {{ "glQueryCounterEXT", "glQueryCounterANGLE" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::query_counter);
    }

    if (IsSupported(GLFeature::query_objects)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBeginQuery, {{ "glBeginQuery" }} },
            { (PRFuncPtr*) &mSymbols.fGenQueries, {{ "glGenQueries" }} },
            { (PRFuncPtr*) &mSymbols.fDeleteQueries, {{ "glDeleteQueries" }} },
            { (PRFuncPtr*) &mSymbols.fEndQuery, {{ "glEndQuery" }} },
            { (PRFuncPtr*) &mSymbols.fGetQueryiv, {{ "glGetQueryiv" }} },
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectuiv, {{ "glGetQueryObjectuiv" }} },
            { (PRFuncPtr*) &mSymbols.fIsQuery, {{ "glIsQuery" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fBeginQuery, {{ "glBeginQueryEXT", "glBeginQueryANGLE" }} },
            { (PRFuncPtr*) &mSymbols.fGenQueries, {{ "glGenQueriesEXT", "glGenQueriesANGLE" }} },
            { (PRFuncPtr*) &mSymbols.fDeleteQueries, {{ "glDeleteQueriesEXT", "glDeleteQueriesANGLE" }} },
            { (PRFuncPtr*) &mSymbols.fEndQuery, {{ "glEndQueryEXT", "glEndQueryANGLE" }} },
            { (PRFuncPtr*) &mSymbols.fGetQueryiv, {{ "glGetQueryivEXT", "glGetQueryivANGLE" }} },
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectuiv, {{ "glGetQueryObjectuivEXT", "glGetQueryObjectuivANGLE" }} },
            { (PRFuncPtr*) &mSymbols.fIsQuery, {{ "glIsQueryEXT", "glIsQueryANGLE" }} },
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
            { (PRFuncPtr*) &mSymbols.fGetQueryObjecti64v, {{ "glGetQueryObjecti64v" }} },
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectui64v, {{ "glGetQueryObjectui64v" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetQueryObjecti64v, {{ "glGetQueryObjecti64vEXT", "glGetQueryObjecti64vANGLE" }} },
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectui64v, {{ "glGetQueryObjectui64vEXT", "glGetQueryObjectui64vANGLE" }} },
            END_SYMBOLS
        };
        if (!fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::get_query_object_i64v)) {
            MarkUnsupported(GLFeature::query_counter);
        }
    }

    if (IsSupported(GLFeature::get_query_object_iv)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectiv, {{ "glGetQueryObjectiv" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetQueryObjectiv, {{ "glGetQueryObjectivEXT", "glGetQueryObjectivANGLE" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::get_query_object_iv);
    }

    if (IsSupported(GLFeature::clear_buffers)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fClearBufferfi,  {{ "glClearBufferfi",  }} },
            { (PRFuncPtr*) &mSymbols.fClearBufferfv,  {{ "glClearBufferfv",  }} },
            { (PRFuncPtr*) &mSymbols.fClearBufferiv,  {{ "glClearBufferiv",  }} },
            { (PRFuncPtr*) &mSymbols.fClearBufferuiv, {{ "glClearBufferuiv" }} },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::clear_buffers);
    }

    if (IsSupported(GLFeature::copy_buffer)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fCopyBufferSubData, {{ "glCopyBufferSubData" }} },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::copy_buffer);
    }

    if (IsSupported(GLFeature::draw_buffers)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawBuffers, {{ "glDrawBuffers" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDrawBuffers, {{ "glDrawBuffersARB", "glDrawBuffersEXT" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::draw_buffers);
    }

    if (IsSupported(GLFeature::get_integer_indexed)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetIntegeri_v, {{ "glGetIntegeri_v" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] ={
            { (PRFuncPtr*) &mSymbols.fGetIntegeri_v, {{ "glGetIntegerIndexedvEXT" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::get_integer_indexed);
    }

    if (IsSupported(GLFeature::get_integer64_indexed)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetInteger64i_v, {{ "glGetInteger64i_v" }} },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::get_integer64_indexed);
    }

    if (IsSupported(GLFeature::gpu_shader4)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetVertexAttribIiv, {{ "glGetVertexAttribIiv", "glGetVertexAttribIivEXT" }} },
            { (PRFuncPtr*) &mSymbols.fGetVertexAttribIuiv, {{ "glGetVertexAttribIuiv", "glGetVertexAttribIuivEXT" }} },
            { (PRFuncPtr*) &mSymbols.fVertexAttribI4i, {{ "glVertexAttribI4i", "glVertexAttribI4iEXT" }} },
            { (PRFuncPtr*) &mSymbols.fVertexAttribI4iv, {{ "glVertexAttribI4iv", "glVertexAttribI4ivEXT" }} },
            { (PRFuncPtr*) &mSymbols.fVertexAttribI4ui, {{ "glVertexAttribI4ui", "glVertexAttribI4uiEXT" }} },
            { (PRFuncPtr*) &mSymbols.fVertexAttribI4uiv, {{ "glVertexAttribI4uiv", "glVertexAttribI4uivEXT" }} },
            { (PRFuncPtr*) &mSymbols.fVertexAttribIPointer, {{ "glVertexAttribIPointer", "glVertexAttribIPointerEXT" }} },
            { (PRFuncPtr*) &mSymbols.fUniform1ui,  {{ "glUniform1ui", "glUniform1uiEXT" }} },
            { (PRFuncPtr*) &mSymbols.fUniform2ui,  {{ "glUniform2ui", "glUniform2uiEXT" }} },
            { (PRFuncPtr*) &mSymbols.fUniform3ui,  {{ "glUniform3ui", "glUniform3uiEXT" }} },
            { (PRFuncPtr*) &mSymbols.fUniform4ui,  {{ "glUniform4ui", "glUniform4uiEXT" }} },
            { (PRFuncPtr*) &mSymbols.fUniform1uiv, {{ "glUniform1uiv", "glUniform1uivEXT" }} },
            { (PRFuncPtr*) &mSymbols.fUniform2uiv, {{ "glUniform2uiv", "glUniform2uivEXT" }} },
            { (PRFuncPtr*) &mSymbols.fUniform3uiv, {{ "glUniform3uiv", "glUniform3uivEXT" }} },
            { (PRFuncPtr*) &mSymbols.fUniform4uiv, {{ "glUniform4uiv", "glUniform4uivEXT" }} },
            { (PRFuncPtr*) &mSymbols.fGetFragDataLocation, {{ "glGetFragDataLocation", "glGetFragDataLocationEXT" }} },
            { (PRFuncPtr*) &mSymbols.fGetUniformuiv, {{ "glGetUniformuiv", "glGetUniformuivEXT" }} },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::gpu_shader4);
    }

    if (IsSupported(GLFeature::map_buffer_range)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fMapBufferRange, {{ "glMapBufferRange" }} },
            { (PRFuncPtr*) &mSymbols.fFlushMappedBufferRange, {{ "glFlushMappedBufferRange" }} },
            { (PRFuncPtr*) &mSymbols.fUnmapBuffer, {{ "glUnmapBuffer" }} },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::map_buffer_range);
    }

    if (IsSupported(GLFeature::texture_3D)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fTexImage3D, {{ "glTexImage3D" }} },
            { (PRFuncPtr*) &mSymbols.fTexSubImage3D, {{ "glTexSubImage3D" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fTexSubImage3D, {{ "glTexSubImage3DEXT", "glTexSubImage3DOES" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_3D);
    }

    if (IsSupported(GLFeature::texture_3D_compressed)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCompressedTexImage3D, {{ "glCompressedTexImage3D" }} },
            { (PRFuncPtr*) &mSymbols.fCompressedTexSubImage3D, {{ "glCompressedTexSubImage3D" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCompressedTexImage3D, {{ "glCompressedTexImage3DARB", "glCompressedTexImage3DOES" }} },
            { (PRFuncPtr*) &mSymbols.fCompressedTexSubImage3D, {{ "glCompressedTexSubImage3DARB", "glCompressedTexSubImage3DOES" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_3D_compressed);
    }

    if (IsSupported(GLFeature::texture_3D_copy)) {
        const SymLoadStruct coreSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCopyTexSubImage3D, {{ "glCopyTexSubImage3D" }} },
            END_SYMBOLS
        };
        const SymLoadStruct extSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCopyTexSubImage3D, {{ "glCopyTexSubImage3DEXT", "glCopyTexSubImage3DOES" }} },
            END_SYMBOLS
        };
        fnLoadFeatureByCore(coreSymbols, extSymbols, GLFeature::texture_3D_copy);
    }

    if (IsSupported(GLFeature::uniform_buffer_object)) {
        // Note: Don't query for glGetActiveUniformName because it is not
        // supported by GL ES 3.
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGetUniformIndices, {{ "glGetUniformIndices" }} },
            { (PRFuncPtr*) &mSymbols.fGetActiveUniformsiv, {{ "glGetActiveUniformsiv" }} },
            { (PRFuncPtr*) &mSymbols.fGetUniformBlockIndex, {{ "glGetUniformBlockIndex" }} },
            { (PRFuncPtr*) &mSymbols.fGetActiveUniformBlockiv, {{ "glGetActiveUniformBlockiv" }} },
            { (PRFuncPtr*) &mSymbols.fGetActiveUniformBlockName, {{ "glGetActiveUniformBlockName" }} },
            { (PRFuncPtr*) &mSymbols.fUniformBlockBinding, {{ "glUniformBlockBinding" }} },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::uniform_buffer_object);
    }

    if (IsSupported(GLFeature::uniform_matrix_nonsquare)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fUniformMatrix2x3fv, {{ "glUniformMatrix2x3fv" }} },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix2x4fv, {{ "glUniformMatrix2x4fv" }} },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix3x2fv, {{ "glUniformMatrix3x2fv" }} },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix3x4fv, {{ "glUniformMatrix3x4fv" }} },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix4x2fv, {{ "glUniformMatrix4x2fv" }} },
            { (PRFuncPtr*) &mSymbols.fUniformMatrix4x3fv, {{ "glUniformMatrix4x3fv" }} },
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
            { (PRFuncPtr*) &mSymbols.fInvalidateFramebuffer,    {{ "glInvalidateFramebuffer" }} },
            { (PRFuncPtr*) &mSymbols.fInvalidateSubFramebuffer, {{ "glInvalidateSubFramebuffer" }} },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::invalidate_framebuffer);
    }

    if (IsSupported(GLFeature::multiview)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fFramebufferTextureMultiview, {{
              "glFramebufferTextureMultiviewOVR",
              "glFramebufferTextureMultiviewLayeredANGLE"
            }} },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::multiview);
    }

    if (IsSupported(GLFeature::prim_restart)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fPrimitiveRestartIndex,    {{ "glPrimitiveRestartIndex", "glPrimitiveRestartIndexNV" }} },
            END_SYMBOLS
        };
        fnLoadForFeature(symbols, GLFeature::prim_restart);
    }

    if (IsExtensionSupported(KHR_debug)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fDebugMessageControl,  {{ "glDebugMessageControl",  "glDebugMessageControlKHR", }} },
            { (PRFuncPtr*) &mSymbols.fDebugMessageInsert,   {{ "glDebugMessageInsert",   "glDebugMessageInsertKHR",  }} },
            { (PRFuncPtr*) &mSymbols.fDebugMessageCallback, {{ "glDebugMessageCallback", "glDebugMessageCallbackKHR" }} },
            { (PRFuncPtr*) &mSymbols.fGetDebugMessageLog,   {{ "glGetDebugMessageLog",   "glGetDebugMessageLogKHR",  }} },
            { (PRFuncPtr*) &mSymbols.fGetPointerv,          {{ "glGetPointerv",          "glGetPointervKHR",         }} },
            { (PRFuncPtr*) &mSymbols.fPushDebugGroup,       {{ "glPushDebugGroup",       "glPushDebugGroupKHR",      }} },
            { (PRFuncPtr*) &mSymbols.fPopDebugGroup,        {{ "glPopDebugGroup",        "glPopDebugGroupKHR",       }} },
            { (PRFuncPtr*) &mSymbols.fObjectLabel,          {{ "glObjectLabel",          "glObjectLabelKHR",         }} },
            { (PRFuncPtr*) &mSymbols.fGetObjectLabel,       {{ "glGetObjectLabel",       "glGetObjectLabelKHR",      }} },
            { (PRFuncPtr*) &mSymbols.fObjectPtrLabel,       {{ "glObjectPtrLabel",       "glObjectPtrLabelKHR",      }} },
            { (PRFuncPtr*) &mSymbols.fGetObjectPtrLabel,    {{ "glGetObjectPtrLabel",    "glGetObjectPtrLabelKHR",   }} },
            END_SYMBOLS
        };
        fnLoadForExt(symbols, KHR_debug);
    }

    if (IsExtensionSupported(NV_fence)) {
        const SymLoadStruct symbols[] = {
            { (PRFuncPtr*) &mSymbols.fGenFences,    {{ "glGenFencesNV"    }} },
            { (PRFuncPtr*) &mSymbols.fDeleteFences, {{ "glDeleteFencesNV" }} },
            { (PRFuncPtr*) &mSymbols.fSetFence,     {{ "glSetFenceNV"     }} },
            { (PRFuncPtr*) &mSymbols.fTestFence,    {{ "glTestFenceNV"    }} },
            { (PRFuncPtr*) &mSymbols.fFinishFence,  {{ "glFinishFenceNV"  }} },
            { (PRFuncPtr*) &mSymbols.fIsFence,      {{ "glIsFenceNV"      }} },
            { (PRFuncPtr*) &mSymbols.fGetFenceiv,   {{ "glGetFenceivNV"   }} },
            END_SYMBOLS
        };
        fnLoadForExt(symbols, NV_fence);
    }

  // clang-format on

  if (IsExtensionSupported(NV_texture_barrier)) {
    const SymLoadStruct symbols[] = {
        {(PRFuncPtr*)&mSymbols.fTextureBarrier, {{"glTextureBarrierNV"}}},
        END_SYMBOLS};
    fnLoadForExt(symbols, NV_texture_barrier);
  }

  if (IsSupported(GLFeature::read_buffer)) {
    const SymLoadStruct symbols[] = {CORE_SYMBOL(ReadBuffer), END_SYMBOLS};
    fnLoadForFeature(symbols, GLFeature::read_buffer);
  }

  if (IsExtensionSupported(APPLE_framebuffer_multisample)) {
    const SymLoadStruct symbols[] = {
        CORE_SYMBOL(ResolveMultisampleFramebufferAPPLE), END_SYMBOLS};
    fnLoadForExt(symbols, APPLE_framebuffer_multisample);
  }

  // Load developer symbols, don't fail if we can't find them.
  const SymLoadStruct devSymbols[] = {CORE_SYMBOL(GetTexImage),
                                      CORE_SYMBOL(GetTexLevelParameteriv),
                                      END_SYMBOLS};
  const bool warnOnFailures = ShouldSpew();
  (void)loader.LoadSymbols(devSymbols, warnOnFailures);
}

#undef CORE_SYMBOL
#undef CORE_EXT_SYMBOL2
#undef EXT_SYMBOL2
#undef EXT_SYMBOL3
#undef END_SYMBOLS

void GLContext::DebugCallback(GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei length,
                              const GLchar* message) {
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
                (uintptr_t)this, id, sourceStr.BeginReading(),
                typeStr.BeginReading(), sevStr.BeginReading(), message);
}

void GLContext::InitExtensions() {
  MOZ_GL_ASSERT(this, IsCurrent());

  std::vector<nsCString> driverExtensionList;

  [&]() {
    if (mSymbols.fGetStringi) {
      GLuint count = 0;
      if (GetPotentialInteger(LOCAL_GL_NUM_EXTENSIONS, (GLint*)&count)) {
        for (GLuint i = 0; i < count; i++) {
          // This is UTF-8.
          const char* rawExt = (const char*)fGetStringi(LOCAL_GL_EXTENSIONS, i);

          // We CANNOT use nsDependentCString here, because the spec doesn't
          // guarantee that the pointers returned are different, only that their
          // contents are. On Flame, each of these index string queries returns
          // the same address.
          driverExtensionList.push_back(nsCString(rawExt));
        }
        return;
      }
    }

    const char* rawExts = (const char*)fGetString(LOCAL_GL_EXTENSIONS);
    if (rawExts) {
      nsDependentCString exts(rawExts);
      SplitByChar(exts, ' ', &driverExtensionList);
    }
  }();
  const auto err = fGetError();
  MOZ_ALWAYS_TRUE(!IsBadCallError(err));

  const bool shouldDumpExts = ShouldDumpExts();
  if (shouldDumpExts) {
    printf_stderr("%i GL driver extensions: (*: recognized)\n",
                  (uint32_t)driverExtensionList.size());
  }

  MarkBitfieldByStrings(driverExtensionList, shouldDumpExts, sExtensionNames,
                        &mAvailableExtensions);

  if (WorkAroundDriverBugs()) {
    if (Vendor() == GLVendor::Qualcomm) {
      // Some Adreno drivers do not report GL_OES_EGL_sync, but they really do
      // support it.
      MarkExtensionSupported(OES_EGL_sync);
    }

    if (Vendor() == GLVendor::ATI) {
      // ATI drivers say this extension exists, but we can't
      // actually find the EGLImageTargetRenderbufferStorageOES
      // extension function pointer in the drivers.
      MarkExtensionUnsupported(OES_EGL_image);
    }

    if (Vendor() == GLVendor::Imagination && Renderer() == GLRenderer::SGX540) {
      // Bug 980048
      MarkExtensionUnsupported(OES_EGL_sync);
    }

#ifdef MOZ_WIDGET_ANDROID
    if (Vendor() == GLVendor::Imagination &&
        Renderer() == GLRenderer::SGX544MP && jni::GetAPIVersion() < 21) {
      // Bug 1026404
      MarkExtensionUnsupported(OES_EGL_image);
      MarkExtensionUnsupported(OES_EGL_image_external);
    }
#endif

    if (Vendor() == GLVendor::ARM && (Renderer() == GLRenderer::Mali400MP ||
                                      Renderer() == GLRenderer::Mali450MP)) {
      // Bug 1264505
      MarkExtensionUnsupported(OES_EGL_image_external);
    }

    if (Renderer() == GLRenderer::AndroidEmulator) {
      // the Android emulator, which we use to run B2G reftests on,
      // doesn't expose the OES_rgb8_rgba8 extension, but it seems to
      // support it (tautologically, as it only runs on desktop GL).
      MarkExtensionSupported(OES_rgb8_rgba8);
    }

    if (Vendor() == GLVendor::VMware &&
        Renderer() == GLRenderer::GalliumLlvmpipe) {
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
    if (Renderer() == GLRenderer::IntelHD3000) {
      MarkExtensionUnsupported(EXT_texture_compression_s3tc);
    }

    // OSX supports EXT_texture_sRGB in Legacy contexts, but not in Core
    // contexts. Though EXT_texture_sRGB was included into GL2.1, it *excludes*
    // the interactions with s3tc. Strictly speaking, you must advertize support
    // for EXT_texture_sRGB in order to allow for srgb+s3tc on desktop GL. The
    // omission of EXT_texture_sRGB in OSX Core contexts appears to be a bug.
    MarkExtensionSupported(EXT_texture_sRGB);
#endif
  }

  if (shouldDumpExts) {
    printf_stderr("\nActivated extensions:\n");

    for (size_t i = 0; i < mAvailableExtensions.size(); i++) {
      if (!mAvailableExtensions[i]) continue;

      const char* ext = sExtensionNames[i];
      printf_stderr("[%i] %s\n", (uint32_t)i, ext);
    }
  }
}

void GLContext::PlatformStartup() {
  RegisterStrongMemoryReporter(new GfxTexturesReporter());
}

// Common code for checking for both GL extensions and GLX extensions.
bool GLContext::ListHasExtension(const GLubyte* extensions,
                                 const char* extension) {
  // fix bug 612572 - we were crashing as we were calling this function with
  // extensions==null
  if (extensions == nullptr || extension == nullptr) return false;

  const GLubyte* start;
  GLubyte* where;
  GLubyte* terminator;

  /* Extension names should not have spaces. */
  where = (GLubyte*)strchr(extension, ' ');
  if (where || *extension == '\0') return false;

  /*
   * It takes a bit of care to be fool-proof about parsing the
   * OpenGL extensions string. Don't be fooled by sub-strings,
   * etc.
   */
  start = extensions;
  for (;;) {
    where = (GLubyte*)strstr((const char*)start, extension);
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

GLFormats GLContext::ChooseGLFormats(const SurfaceCaps& caps) const {
  GLFormats formats;

  // If we're on ES2 hardware and we have an explicit request for 16 bits of
  // color or less OR we don't support full 8-bit color, return a 4444 or 565
  // format.
  bool bpp16 = caps.bpp16;
  if (IsGLES()) {
    if (!IsExtensionSupported(OES_rgb8_rgba8)) bpp16 = true;
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
      formats.color_texType = LOCAL_GL_UNSIGNED_SHORT_4_4_4_4;
      formats.color_rbFormat = LOCAL_GL_RGBA4;
    } else {
      formats.color_texInternalFormat = LOCAL_GL_RGB;
      formats.color_texFormat = LOCAL_GL_RGB;
      formats.color_texType = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
      formats.color_rbFormat = LOCAL_GL_RGB565;
    }
  } else {
    formats.color_texType = LOCAL_GL_UNSIGNED_BYTE;

    if (caps.alpha) {
      formats.color_texInternalFormat =
          IsGLES() ? LOCAL_GL_RGBA : LOCAL_GL_RGBA8;
      formats.color_texFormat = LOCAL_GL_RGBA;
      formats.color_rbFormat = LOCAL_GL_RGBA8;
    } else {
      formats.color_texInternalFormat = IsGLES() ? LOCAL_GL_RGB : LOCAL_GL_RGB8;
      formats.color_texFormat = LOCAL_GL_RGB;
      formats.color_rbFormat = LOCAL_GL_RGB8;
    }
  }

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

bool GLContext::IsFramebufferComplete(GLuint fb, GLenum* pStatus) {
  MOZ_ASSERT(fb);

  ScopedBindFramebuffer autoFB(this, fb);
  MOZ_GL_ASSERT(this, fIsFramebuffer(fb));

  GLenum status = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (pStatus) *pStatus = status;

  return status == LOCAL_GL_FRAMEBUFFER_COMPLETE;
}

void GLContext::AttachBuffersToFB(GLuint colorTex, GLuint colorRB,
                                  GLuint depthRB, GLuint stencilRB, GLuint fb,
                                  GLenum target) {
  MOZ_ASSERT(fb);
  MOZ_ASSERT(!(colorTex && colorRB));

  ScopedBindFramebuffer autoFB(this, fb);
  MOZ_GL_ASSERT(this, fIsFramebuffer(fb));  // It only counts after being bound.

  if (colorTex) {
    MOZ_GL_ASSERT(this, fIsTexture(colorTex));
    MOZ_ASSERT(target == LOCAL_GL_TEXTURE_2D ||
               target == LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                          target, colorTex, 0);
  } else if (colorRB) {
    // On the Android 4.3 emulator, IsRenderbuffer may return false incorrectly.
    MOZ_GL_ASSERT(this, fIsRenderbuffer(colorRB) ||
                            Renderer() == GLRenderer::AndroidEmulator);
    fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                             LOCAL_GL_RENDERBUFFER, colorRB);
  }

  if (depthRB) {
    MOZ_GL_ASSERT(this, fIsRenderbuffer(depthRB) ||
                            Renderer() == GLRenderer::AndroidEmulator);
    fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                             LOCAL_GL_RENDERBUFFER, depthRB);
  }

  if (stencilRB) {
    MOZ_GL_ASSERT(this, fIsRenderbuffer(stencilRB) ||
                            Renderer() == GLRenderer::AndroidEmulator);
    fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                             LOCAL_GL_RENDERBUFFER, stencilRB);
  }
}

bool GLContext::AssembleOffscreenFBs(const GLuint colorMSRB,
                                     const GLuint depthRB,
                                     const GLuint stencilRB,
                                     const GLuint texture, GLuint* drawFB_out,
                                     GLuint* readFB_out) {
  if (!colorMSRB && !texture) {
    MOZ_ASSERT(!depthRB && !stencilRB);

    if (drawFB_out) *drawFB_out = 0;
    if (readFB_out) *readFB_out = 0;

    return true;
  }

  ScopedBindFramebuffer autoFB(this);

  GLuint drawFB = 0;
  GLuint readFB = 0;

  if (texture) {
    readFB = 0;
    fGenFramebuffers(1, &readFB);
    BindFB(readFB);
    fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                          LOCAL_GL_TEXTURE_2D, texture, 0);
  }

  if (colorMSRB) {
    drawFB = 0;
    fGenFramebuffers(1, &drawFB);
    BindFB(drawFB);
    fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                             LOCAL_GL_RENDERBUFFER, colorMSRB);
  } else {
    drawFB = readFB;
  }
  MOZ_ASSERT(GetFB() == drawFB);

  if (depthRB) {
    fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                             LOCAL_GL_RENDERBUFFER, depthRB);
  }

  if (stencilRB) {
    fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                             LOCAL_GL_RENDERBUFFER, stencilRB);
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
    MOZ_CRASH("drawFB created when not requested!");
  }

  if (readFB_out) {
    *readFB_out = readFB;
  } else if (readFB) {
    MOZ_CRASH("readFB created when not requested!");
  }

  return isComplete;
}

void GLContext::MarkDestroyed() {
  if (IsDestroyed()) return;

  OnMarkDestroyed();

  // Null these before they're naturally nulled after dtor, as we want GLContext
  // to still be alive in *their* dtors.
  mScreen = nullptr;
  mBlitHelper = nullptr;
  mReadTexImageHelper = nullptr;

  mContextLost = true;
  mSymbols = {};
}

// -

#ifdef MOZ_GL_DEBUG
/* static */
void GLContext::AssertNotPassingStackBufferToTheGL(const void* ptr) {
  int somethingOnTheStack;
  const void* someStackPtr = &somethingOnTheStack;
  const int page_bits = 12;
  intptr_t page = reinterpret_cast<uintptr_t>(ptr) >> page_bits;
  intptr_t someStackPage =
      reinterpret_cast<uintptr_t>(someStackPtr) >> page_bits;
  uintptr_t pageDistance = std::abs(page - someStackPage);

  // Explanation for the "distance <= 1" check here as opposed to just
  // an equality check.
  //
  // Here we assume that pages immediately adjacent to the someStackAddress
  // page, are also stack pages. That allows to catch the case where the calling
  // frame put a buffer on the stack, and we just crossed the page boundary.
  // That is likely to happen, precisely, when using stack arrays. I hit that
  // specifically with CompositorOGL::Initialize.
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

void GLContext::CreatedProgram(GLContext* aOrigin, GLuint aName) {
  mTrackedPrograms.AppendElement(NamedResource(aOrigin, aName));
}

void GLContext::CreatedShader(GLContext* aOrigin, GLuint aName) {
  mTrackedShaders.AppendElement(NamedResource(aOrigin, aName));
}

void GLContext::CreatedBuffers(GLContext* aOrigin, GLsizei aCount,
                               GLuint* aNames) {
  for (GLsizei i = 0; i < aCount; ++i) {
    mTrackedBuffers.AppendElement(NamedResource(aOrigin, aNames[i]));
  }
}

void GLContext::CreatedQueries(GLContext* aOrigin, GLsizei aCount,
                               GLuint* aNames) {
  for (GLsizei i = 0; i < aCount; ++i) {
    mTrackedQueries.AppendElement(NamedResource(aOrigin, aNames[i]));
  }
}

void GLContext::CreatedTextures(GLContext* aOrigin, GLsizei aCount,
                                GLuint* aNames) {
  for (GLsizei i = 0; i < aCount; ++i) {
    mTrackedTextures.AppendElement(NamedResource(aOrigin, aNames[i]));
  }
}

void GLContext::CreatedFramebuffers(GLContext* aOrigin, GLsizei aCount,
                                    GLuint* aNames) {
  for (GLsizei i = 0; i < aCount; ++i) {
    mTrackedFramebuffers.AppendElement(NamedResource(aOrigin, aNames[i]));
  }
}

void GLContext::CreatedRenderbuffers(GLContext* aOrigin, GLsizei aCount,
                                     GLuint* aNames) {
  for (GLsizei i = 0; i < aCount; ++i) {
    mTrackedRenderbuffers.AppendElement(NamedResource(aOrigin, aNames[i]));
  }
}

static void RemoveNamesFromArray(GLContext* aOrigin, GLsizei aCount,
                                 const GLuint* aNames,
                                 nsTArray<GLContext::NamedResource>& aArray) {
  for (GLsizei j = 0; j < aCount; ++j) {
    GLuint name = aNames[j];
    // name 0 can be ignored
    if (name == 0) continue;

    for (uint32_t i = 0; i < aArray.Length(); ++i) {
      if (aArray[i].name == name) {
        aArray.RemoveElementAt(i);
        break;
      }
    }
  }
}

void GLContext::DeletedProgram(GLContext* aOrigin, GLuint aName) {
  RemoveNamesFromArray(aOrigin, 1, &aName, mTrackedPrograms);
}

void GLContext::DeletedShader(GLContext* aOrigin, GLuint aName) {
  RemoveNamesFromArray(aOrigin, 1, &aName, mTrackedShaders);
}

void GLContext::DeletedBuffers(GLContext* aOrigin, GLsizei aCount,
                               const GLuint* aNames) {
  RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedBuffers);
}

void GLContext::DeletedQueries(GLContext* aOrigin, GLsizei aCount,
                               const GLuint* aNames) {
  RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedQueries);
}

void GLContext::DeletedTextures(GLContext* aOrigin, GLsizei aCount,
                                const GLuint* aNames) {
  RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedTextures);
}

void GLContext::DeletedFramebuffers(GLContext* aOrigin, GLsizei aCount,
                                    const GLuint* aNames) {
  RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedFramebuffers);
}

void GLContext::DeletedRenderbuffers(GLContext* aOrigin, GLsizei aCount,
                                     const GLuint* aNames) {
  RemoveNamesFromArray(aOrigin, aCount, aNames, mTrackedRenderbuffers);
}

static void MarkContextDestroyedInArray(
    GLContext* aContext, nsTArray<GLContext::NamedResource>& aArray) {
  for (uint32_t i = 0; i < aArray.Length(); ++i) {
    if (aArray[i].origin == aContext) aArray[i].originDeleted = true;
  }
}

void GLContext::SharedContextDestroyed(GLContext* aChild) {
  MarkContextDestroyedInArray(aChild, mTrackedPrograms);
  MarkContextDestroyedInArray(aChild, mTrackedShaders);
  MarkContextDestroyedInArray(aChild, mTrackedTextures);
  MarkContextDestroyedInArray(aChild, mTrackedFramebuffers);
  MarkContextDestroyedInArray(aChild, mTrackedRenderbuffers);
  MarkContextDestroyedInArray(aChild, mTrackedBuffers);
  MarkContextDestroyedInArray(aChild, mTrackedQueries);
}

static void ReportArrayContents(
    const char* title, const nsTArray<GLContext::NamedResource>& aArray) {
  if (aArray.Length() == 0) return;

  printf_stderr("%s:\n", title);

  nsTArray<GLContext::NamedResource> copy(aArray);
  copy.Sort();

  GLContext* lastContext = nullptr;
  for (uint32_t i = 0; i < copy.Length(); ++i) {
    if (lastContext != copy[i].origin) {
      if (lastContext) printf_stderr("\n");
      printf_stderr("  [%p - %s] ", copy[i].origin,
                    copy[i].originDeleted ? "deleted" : "live");
      lastContext = copy[i].origin;
    }
    printf_stderr("%d ", copy[i].name);
  }
  printf_stderr("\n");
}

void GLContext::ReportOutstandingNames() {
  if (!ShouldSpew()) return;

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

void GLContext::GuaranteeResolve() { fFinish(); }

const gfx::IntSize& GLContext::OffscreenSize() const {
  MOZ_ASSERT(IsOffscreen());
  return mScreen->Size();
}

bool GLContext::CreateScreenBufferImpl(const IntSize& size,
                                       const SurfaceCaps& caps) {
  UniquePtr<GLScreenBuffer> newScreen =
      GLScreenBuffer::Create(this, size, caps);
  if (!newScreen) return false;

  if (!newScreen->Resize(size)) {
    return false;
  }

  // This will rebind to 0 (Screen) if needed when
  // it falls out of scope.
  ScopedBindFramebuffer autoFB(this);

  mScreen = std::move(newScreen);

  return true;
}

bool GLContext::ResizeScreenBuffer(const IntSize& size) {
  if (!IsOffscreenSizeAllowed(size)) return false;

  return mScreen->Resize(size);
}

void GLContext::ForceDirtyScreen() {
  ScopedBindFramebuffer autoFB(0);

  BeforeGLDrawCall();
  // no-op; just pretend we did something
  AfterGLDrawCall();
}

void GLContext::CleanDirtyScreen() {
  ScopedBindFramebuffer autoFB(0);

  BeforeGLReadCall();
  // no-op; we just want to make sure the Read FBO is updated if it needs to be
  AfterGLReadCall();
}

bool GLContext::IsOffscreenSizeAllowed(const IntSize& aSize) const {
  int32_t biggerDimension = std::max(aSize.width, aSize.height);
  int32_t maxAllowed = std::min(mMaxRenderbufferSize, mMaxTextureSize);
  return biggerDimension <= maxAllowed;
}

bool GLContext::IsOwningThreadCurrent() {
  return PlatformThread::CurrentId() == mOwningThreadId;
}

GLBlitHelper* GLContext::BlitHelper() {
  if (!mBlitHelper) {
    mBlitHelper.reset(new GLBlitHelper(this));
  }

  return mBlitHelper.get();
}

GLReadTexImageHelper* GLContext::ReadTexImageHelper() {
  if (!mReadTexImageHelper) {
    mReadTexImageHelper = MakeUnique<GLReadTexImageHelper>(this);
  }

  return mReadTexImageHelper.get();
}

void GLContext::FlushIfHeavyGLCallsSinceLastFlush() {
  if (!mHeavyGLCallsSinceLastFlush) {
    return;
  }
  if (MakeCurrent()) {
    fFlush();
  }
}

/*static*/
bool GLContext::ShouldDumpExts() { return gfxEnv::GlDumpExtensions(); }

bool DoesStringMatch(const char* aString, const char* aWantedString) {
  if (!aString || !aWantedString) return false;

  const char* occurrence = strstr(aString, aWantedString);

  // aWanted not found
  if (!occurrence) return false;

  // aWantedString preceded by alpha character
  if (occurrence != aString && isalpha(*(occurrence - 1))) return false;

  // aWantedVendor followed by alpha character
  const char* afterOccurrence = occurrence + strlen(aWantedString);
  if (isalpha(*afterOccurrence)) return false;

  return true;
}

/*static*/
bool GLContext::ShouldSpew() { return gfxEnv::GlSpew(); }

void SplitByChar(const nsACString& str, const char delim,
                 std::vector<nsCString>* const out) {
  uint32_t start = 0;
  while (true) {
    int32_t end = str.FindChar(' ', start);
    if (end == -1) break;

    uint32_t len = (uint32_t)end - start;
    nsDependentCSubstring substr(str, start, len);
    out->push_back(nsCString(substr));

    start = end + 1;
  }

  nsDependentCSubstring substr(str, start);
  out->push_back(nsCString(substr));
}

bool GLContext::Readback(SharedSurface* src, gfx::DataSourceSurface* dest) {
  MOZ_ASSERT(src && dest);
  MOZ_ASSERT(dest->GetSize() == src->mSize);

  if (!MakeCurrent()) {
    return false;
  }

  SharedSurface* prev = GetLockedSurface();

  const bool needsSwap = src != prev;
  if (needsSwap) {
    if (prev) prev->UnlockProd();
    src->LockProd();
  }

  GLuint tempFB = 0;
  GLuint tempTex = 0;

  {
    ScopedBindFramebuffer autoFB(this);

    // We're consuming from the producer side, so which do we use?
    // Really, we just want a read-only lock, so ConsumerAcquire is the best
    // match.
    src->ProducerReadAcquire();

    if (src->mAttachType == AttachmentType::Screen) {
      fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
    } else {
      fGenFramebuffers(1, &tempFB);
      fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, tempFB);

      switch (src->mAttachType) {
        case AttachmentType::GLTexture:
          fFramebufferTexture2D(
              LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
              src->ProdTextureTarget(), src->ProdTexture(), 0);
          break;
        case AttachmentType::GLRenderbuffer:
          fFramebufferRenderbuffer(
              LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
              LOCAL_GL_RENDERBUFFER, src->ProdRenderbuffer());
          break;
        default:
          MOZ_CRASH("GFX: bad `src->mAttachType`.");
      }

      DebugOnly<GLenum> status = fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
      MOZ_GL_ASSERT(this, status == LOCAL_GL_FRAMEBUFFER_COMPLETE);
    }

    if (src->NeedsIndirectReads()) {
      fGenTextures(1, &tempTex);
      {
        ScopedBindTexture autoTex(this, tempTex);

        GLenum format = src->mHasAlpha ? LOCAL_GL_RGBA : LOCAL_GL_RGB;
        auto width = src->mSize.width;
        auto height = src->mSize.height;
        fCopyTexImage2D(LOCAL_GL_TEXTURE_2D, 0, format, 0, 0, width, height, 0);
      }

      fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                            LOCAL_GL_TEXTURE_2D, tempTex, 0);
    }

    ReadPixelsIntoDataSurface(this, dest);

    src->ProducerReadRelease();
  }

  if (tempFB) fDeleteFramebuffers(1, &tempFB);

  if (tempTex) {
    fDeleteTextures(1, &tempTex);
  }

  if (needsSwap) {
    src->UnlockProd();
    if (prev) prev->LockProd();
  }

  return true;
}

void GLContext::fBindFramebuffer(GLenum target, GLuint framebuffer) {
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

void GLContext::fCopyTexImage2D(GLenum target, GLint level,
                                GLenum internalformat, GLint x, GLint y,
                                GLsizei width, GLsizei height, GLint border) {
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
    didCopyTexImage2D = mScreen->CopyTexImage2D(target, level, internalformat,
                                                x, y, width, height, border);
  }

  if (!didCopyTexImage2D) {
    raw_fCopyTexImage2D(target, level, internalformat, x, y, width, height,
                        border);
  }
  AfterGLReadCall();
}

void GLContext::fGetIntegerv(GLenum pname, GLint* params) {
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
      MOZ_ASSERT(mMaxTextureSize > 0);
      *params = mMaxTextureSize;
      break;

    case LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      MOZ_ASSERT(mMaxCubeMapTextureSize > 0);
      *params = mMaxCubeMapTextureSize;
      break;

    case LOCAL_GL_MAX_RENDERBUFFER_SIZE:
      MOZ_ASSERT(mMaxRenderbufferSize > 0);
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

void GLContext::fReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                            GLenum format, GLenum type, GLvoid* pixels) {
  BeforeGLReadCall();

  bool didReadPixels = false;
  if (mScreen) {
    didReadPixels =
        mScreen->ReadPixels(x, y, width, height, format, type, pixels);
  }

  if (!didReadPixels) {
    raw_fReadPixels(x, y, width, height, format, type, pixels);
  }

  AfterGLReadCall();

  // Check if GL is giving back 1.0 alpha for
  // RGBA reads to RGBA images from no-alpha buffers.
#ifdef XP_MACOSX
  if (WorkAroundDriverBugs() && Vendor() == gl::GLVendor::NVIDIA &&
      format == LOCAL_GL_RGBA && type == LOCAL_GL_UNSIGNED_BYTE &&
      !IsCoreProfile() && width && height) {
    GLint alphaBits = 0;
    fGetIntegerv(LOCAL_GL_ALPHA_BITS, &alphaBits);
    if (!alphaBits) {
      const uint32_t alphaMask = 0xff000000;

      uint32_t* itr = (uint32_t*)pixels;
      uint32_t testPixel = *itr;
      if ((testPixel & alphaMask) != alphaMask) {
        // We need to set the alpha channel to 1.0 manually.
        uint32_t* itrEnd =
            itr + width * height;  // Stride is guaranteed to be width*4.

        for (; itr != itrEnd; itr++) {
          *itr |= alphaMask;
        }
      }
    }
  }
#endif
}

void GLContext::fDeleteFramebuffers(GLsizei n, const GLuint* names) {
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
static bool WillTextureMapSucceed(GLsizei width, GLsizei height, GLenum format,
                                  GLenum type) {
  bool willSucceed = false;
  // Some drivers leave large gaps between textures, so require
  // there to be double the actual size of the texture available.
  size_t size = width * height * GetBytesPerTexel(format, type) * 2;

  void* p = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p != MAP_FAILED) {
    willSucceed = true;
    munmap(p, size);
  }

  return willSucceed;
}
#endif  // MOZ_WIDGET_ANDROID

void GLContext::fTexImage2D(GLenum target, GLint level, GLint internalformat,
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
  raw_fTexImage2D(target, level, internalformat, width, height, border, format,
                  type, pixels);
}

GLuint GLContext::GetDrawFB() {
  if (mScreen) return mScreen->GetDrawFB();

  GLuint ret = 0;
  GetUIntegerv(LOCAL_GL_DRAW_FRAMEBUFFER_BINDING_EXT, &ret);
  return ret;
}

GLuint GLContext::GetReadFB() {
  if (mScreen) return mScreen->GetReadFB();

  GLenum bindEnum = IsSupported(GLFeature::split_framebuffer)
                        ? LOCAL_GL_READ_FRAMEBUFFER_BINDING_EXT
                        : LOCAL_GL_FRAMEBUFFER_BINDING;

  GLuint ret = 0;
  GetUIntegerv(bindEnum, &ret);
  return ret;
}

GLuint GLContext::GetFB() {
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

bool GLContext::InitOffscreen(const gfx::IntSize& size,
                              const SurfaceCaps& caps) {
  if (!CreateScreenBuffer(size, caps)) return false;

  if (!MakeCurrent()) {
    return false;
  }
  fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
  fScissor(0, 0, size.width, size.height);
  fViewport(0, 0, size.width, size.height);

  mCaps = mScreen->mCaps;
  MOZ_ASSERT(!mCaps.any);

  return true;
}

GLuint CreateTexture(GLContext* aGL, GLenum aInternalFormat, GLenum aFormat,
                     GLenum aType, const gfx::IntSize& aSize, bool linear) {
  GLuint tex = 0;
  aGL->fGenTextures(1, &tex);
  ScopedBindTexture autoTex(aGL, tex);

  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                      linear ? LOCAL_GL_LINEAR : LOCAL_GL_NEAREST);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                      linear ? LOCAL_GL_LINEAR : LOCAL_GL_NEAREST);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                      LOCAL_GL_CLAMP_TO_EDGE);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                      LOCAL_GL_CLAMP_TO_EDGE);

  aGL->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, aInternalFormat, aSize.width,
                   aSize.height, 0, aFormat, aType, nullptr);

  return tex;
}

GLuint CreateTextureForOffscreen(GLContext* aGL, const GLFormats& aFormats,
                                 const gfx::IntSize& aSize) {
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

uint32_t GetBytesPerTexel(GLenum format, GLenum type) {
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
    if (type == LOCAL_GL_UNSIGNED_INT_24_8_EXT) return 4;
  }

  if (type == LOCAL_GL_UNSIGNED_BYTE || type == LOCAL_GL_FLOAT ||
      type == LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV) {
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
      case LOCAL_GL_BGRA_EXT:
        return 4 * multiplier;
      default:
        break;
    }
  } else if (type == LOCAL_GL_UNSIGNED_SHORT_4_4_4_4 ||
             type == LOCAL_GL_UNSIGNED_SHORT_5_5_5_1 ||
             type == LOCAL_GL_UNSIGNED_SHORT_5_6_5 ||
             type == LOCAL_GL_UNSIGNED_SHORT) {
    return 2;
  }

  gfxCriticalError() << "Unknown texture type " << type << " or format "
                     << format;
  return 0;
}

bool GLContext::MakeCurrent(bool aForce) const {
  if (MOZ_UNLIKELY(IsContextLost())) return false;

  if (MOZ_LIKELY(!aForce)) {
    bool isCurrent;
    if (mUseTLSIsCurrent) {
      isCurrent = (sCurrentContext.get() == reinterpret_cast<uintptr_t>(this));
    } else {
      isCurrent = IsCurrentImpl();
    }
    if (MOZ_LIKELY(isCurrent)) {
      MOZ_ASSERT(IsCurrentImpl() ||
                 !MakeCurrentImpl());  // Might have lost context.
      return true;
    }
  }

  if (!MakeCurrentImpl()) return false;

  sCurrentContext.set(reinterpret_cast<uintptr_t>(this));
  return true;
}

void GLContext::ResetSyncCallCount(const char* resetReason) const {
  if (ShouldSpew()) {
    printf_stderr("On %s, mSyncGLCallCount = %" PRIu64 "\n", resetReason,
                  mSyncGLCallCount);
  }

  mSyncGLCallCount = 0;
}

// -

bool CheckContextLost(const GLContext* const gl) {
  return gl->CheckContextLost();
}

// -

GLenum GLContext::GetError() const {
  if (mContextLost) return LOCAL_GL_CONTEXT_LOST;

  if (mImplicitMakeCurrent) {
    (void)MakeCurrent();
  }

  const auto fnGetError = [&]() {
    const auto ret = mSymbols.fGetError();
    if (ret == LOCAL_GL_CONTEXT_LOST) {
      OnContextLostError();
      mTopError = ret;  // Promote to top!
    }
    return ret;
  };

  auto ret = fnGetError();

  {
    auto flushedErr = ret;
    uint32_t i = 1;
    while (flushedErr && !mContextLost) {
      if (i == 100) {
        gfxCriticalError() << "Flushing glGetError still "
                           << gfx::hexa(flushedErr) << " after " << i
                           << " calls.";
        break;
      }
      flushedErr = fnGetError();
      i += 1;
    }
  }

  if (mTopError) {
    ret = mTopError;
    mTopError = 0;
  }

  if (mDebugFlags & DebugFlagTrace) {
    const auto errStr = GLErrorToString(ret);
    printf_stderr("[gl:%p] GetError() -> %s\n", this, errStr.c_str());
  }
  return ret;
}

GLenum GLContext::fGetGraphicsResetStatus() const {
  OnSyncCall();

  GLenum ret = 0;
  if (mSymbols.fGetGraphicsResetStatus) {
    if (mImplicitMakeCurrent) {
      (void)MakeCurrent();
    }
    ret = mSymbols.fGetGraphicsResetStatus();
  } else {
    if (!MakeCurrent(true)) {
      ret = LOCAL_GL_UNKNOWN_CONTEXT_RESET_ARB;
    }
  }

  if (mDebugFlags & DebugFlagTrace) {
    printf_stderr("[gl:%p] GetGraphicsResetStatus() -> 0x%04x\n", this, ret);
  }

  return ret;
}

void GLContext::OnContextLostError() const {
  if (mDebugFlags & DebugFlagTrace) {
    printf_stderr("[gl:%p] CONTEXT_LOST\n", this);
  }
  mContextLost = true;
}

// --

/*static*/ std::string GLContext::GLErrorToString(const GLenum err) {
  switch (err) {
    case LOCAL_GL_NO_ERROR:
      return "GL_NO_ERROR";
    case LOCAL_GL_INVALID_ENUM:
      return "GL_INVALID_ENUM";
    case LOCAL_GL_INVALID_VALUE:
      return "GL_INVALID_VALUE";
    case LOCAL_GL_INVALID_OPERATION:
      return "GL_INVALID_OPERATION";
    case LOCAL_GL_STACK_OVERFLOW:
      return "GL_STACK_OVERFLOW";
    case LOCAL_GL_STACK_UNDERFLOW:
      return "GL_STACK_UNDERFLOW";
    case LOCAL_GL_OUT_OF_MEMORY:
      return "GL_OUT_OF_MEMORY";
    case LOCAL_GL_TABLE_TOO_LARGE:
      return "GL_TABLE_TOO_LARGE";
    case LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION:
      return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case LOCAL_GL_CONTEXT_LOST:
      return "GL_CONTEXT_LOST";
  }

  const nsPrintfCString hex("<enum 0x%04x>", err);
  return hex.BeginReading();
}

// --

void GLContext::BeforeGLCall_Debug(const char* const funcName) const {
  MOZ_ASSERT(mDebugFlags);

  if (mDebugFlags & DebugFlagTrace) {
    printf_stderr("[gl:%p] > %s\n", this, funcName);
  }

  MOZ_ASSERT(!mDebugErrorScope);
  mDebugErrorScope.reset(new LocalErrorScope(*this));
}

void GLContext::AfterGLCall_Debug(const char* const funcName) const {
  MOZ_ASSERT(mDebugFlags);

  // calling fFinish() immediately after every GL call makes sure that if this
  // GL command crashes, the stack trace will actually point to it. Otherwise,
  // OpenGL being an asynchronous API, stack traces tend to be meaningless
  mSymbols.fFinish();

  const auto err = mDebugErrorScope->GetError();
  mDebugErrorScope = nullptr;
  if (!mTopError) {
    mTopError = err;
  }

  if (mDebugFlags & DebugFlagTrace) {
    printf_stderr("[gl:%p] < %s [%s]\n", this, funcName,
                  GLErrorToString(err).c_str());
  }

  if (err && !mLocalErrorScopeStack.size()) {
    const auto errStr = GLErrorToString(err);
    const auto text = nsPrintfCString("%s: Generated unexpected %s error",
                                      funcName, errStr.c_str());
    printf_stderr("[gl:%p] %s.\n", this, text.BeginReading());

    const bool abortOnError = mDebugFlags & DebugFlagAbortOnError;
    if (abortOnError && err != LOCAL_GL_CONTEXT_LOST) {
      gfxCriticalErrorOnce() << text.BeginReading();
      MOZ_CRASH(
          "Aborting... (Run with MOZ_GL_DEBUG_ABORT_ON_ERROR=0 to disable)");
    }
  }
}

/*static*/
void GLContext::OnImplicitMakeCurrentFailure(const char* const funcName) {
  gfxCriticalError() << "Ignoring call to " << funcName << " with failed"
                     << " mImplicitMakeCurrent.";
}

// -

// These are defined out of line so that we don't need to include
// ISurfaceAllocator.h in SurfaceTypes.h.
SurfaceCaps::SurfaceCaps() = default;
SurfaceCaps::SurfaceCaps(const SurfaceCaps& other) = default;
SurfaceCaps& SurfaceCaps::operator=(const SurfaceCaps& other) = default;
SurfaceCaps::~SurfaceCaps() = default;

} /* namespace gl */
} /* namespace mozilla */
