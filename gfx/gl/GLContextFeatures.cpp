/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace gl {

const size_t kMAX_EXTENSION_GROUP_SIZE = 5;

enum class GLVersion : uint32_t {
  NONE = 0,  // Feature is not supported natively by GL
  GL1_2 = 120,
  GL1_3 = 130,
  GL2 = 200,
  GL2_1 = 210,
  GL3 = 300,
  GL3_1 = 310,
  GL3_2 = 320,
  GL3_3 = 330,
  GL4 = 400,
  GL4_1 = 410,
  GL4_2 = 420,
  GL4_3 = 430,
};

enum class GLESVersion : uint32_t {
  NONE = 0,  // Feature is not support natively by GL ES
  ES2 = 200,
  ES3 = 300,
  ES3_1 = 310,
  ES3_2 = 320,
};

// ARB_ES2_compatibility is natively supported in OpenGL 4.1.
static const GLVersion kGLCoreVersionForES2Compat = GLVersion::GL4_1;

// ARB_ES3_compatibility is natively supported in OpenGL 4.3.
static const GLVersion kGLCoreVersionForES3Compat = GLVersion::GL4_3;

struct FeatureInfo {
  const char* mName;

  /* The (desktop) OpenGL version that provides this feature */
  GLVersion mOpenGLVersion;

  /* The OpenGL ES version that provides this feature */
  GLESVersion mOpenGLESVersion;

  /* If there is an ARB extension, and its function symbols are
   * not decorated with an ARB suffix, then its extension ID should go
   * here, and NOT in mExtensions.  For example, ARB_vertex_array_object
   * functions do not have an ARB suffix, because it is an extension that
   * was created to match core GL functionality and will never differ.
   * Some ARB extensions do have a suffix, if they were created before
   * a core version of the functionality existed.
   *
   * If there is no such ARB extension, pass 0 (GLContext::Extension_None)
   */
  GLContext::GLExtensions mARBExtensionWithoutARBSuffix;

  /* Extensions that also provide this feature */
  GLContext::GLExtensions mExtensions[kMAX_EXTENSION_GROUP_SIZE];
};

static const FeatureInfo sFeatureInfoArr[] = {
    {"bind_buffer_offset",
     GLVersion::NONE,
     GLESVersion::NONE,
     GLContext::Extension_None,
     {

         GLContext::EXT_transform_feedback, GLContext::NV_transform_feedback2,
         GLContext::Extensions_End}},
    {"blend_minmax",
     GLVersion::GL2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::EXT_blend_minmax, GLContext::Extensions_End}},
    {"clear_buffers",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::Extensions_End}},
    {"copy_buffer",
     GLVersion::GL3_1,
     GLESVersion::ES3,
     GLContext::ARB_copy_buffer,
     {GLContext::Extensions_End}},
    {"depth_texture",
     GLVersion::GL2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_depth_texture, GLContext::OES_depth_texture,
      // Intentionally avoid putting ANGLE_depth_texture here,
      // it does not offer quite the same functionality.
      GLContext::Extensions_End}},
    {"draw_buffers",
     GLVersion::GL2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_draw_buffers, GLContext::EXT_draw_buffers,
      GLContext::Extensions_End}},
    {"draw_buffers_indexed",
     GLVersion::GL3,
     GLESVersion::ES3_2,
     GLContext::Extension_None,
     {GLContext::OES_draw_buffers_indexed, GLContext::Extensions_End}},
    {"draw_instanced",
     GLVersion::GL3_1,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_draw_instanced, GLContext::EXT_draw_instanced,
      GLContext::NV_draw_instanced, GLContext::ANGLE_instanced_arrays,
      GLContext::Extensions_End}},
    {"element_index_uint",
     GLVersion::GL2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::OES_element_index_uint, GLContext::Extensions_End}},
    {"ES2_compatibility",
     kGLCoreVersionForES2Compat,
     GLESVersion::ES2,                  // OpenGL ES version
     GLContext::ARB_ES2_compatibility,  // no suffix on ARB extension
     {GLContext::Extensions_End}},
    {"ES3_compatibility",
     kGLCoreVersionForES3Compat,
     GLESVersion::ES3,                  // OpenGL ES version
     GLContext::ARB_ES3_compatibility,  // no suffix on ARB extension
     {GLContext::Extensions_End}},
    {"EXT_color_buffer_float",
     GLVersion::GL3,
     GLESVersion::ES3_2,
     GLContext::Extension_None,
     {GLContext::EXT_color_buffer_float, GLContext::Extensions_End}},
    {// Removes clamping for float color outputs from frag shaders.
     "frag_color_float",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_color_buffer_float, GLContext::EXT_color_buffer_float,
      GLContext::EXT_color_buffer_half_float,
      GLContext::CHROMIUM_color_buffer_float_rgba, GLContext::Extensions_End}},
    {"frag_depth",
     GLVersion::GL2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::EXT_frag_depth, GLContext::Extensions_End}},
    {// Check for just the blit framebuffer blit part of
     // ARB_framebuffer_object
     "framebuffer_blit",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::ARB_framebuffer_object,
     {GLContext::ANGLE_framebuffer_blit, GLContext::EXT_framebuffer_blit,
      GLContext::NV_framebuffer_blit, GLContext::Extensions_End}},
    {// Check for just the multisample renderbuffer part of
     // ARB_framebuffer_object
     "framebuffer_multisample",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::ARB_framebuffer_object,
     {GLContext::ANGLE_framebuffer_multisample,
      GLContext::APPLE_framebuffer_multisample,
      GLContext::EXT_framebuffer_multisample,
      GLContext::EXT_multisampled_render_to_texture,
      GLContext::Extensions_End}},
    {// ARB_framebuffer_object support
     "framebuffer_object",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::ARB_framebuffer_object,
     {GLContext::Extensions_End}},
    {// EXT_framebuffer_object/OES_framebuffer_object support
     "framebuffer_object_EXT_OES",
     GLVersion::GL3,
     GLESVersion::ES2,
     GLContext::Extension_None,
     {GLContext::EXT_framebuffer_object, GLContext::OES_framebuffer_object,
      GLContext::Extensions_End}},
    {"get_integer_indexed",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::EXT_draw_buffers2, GLContext::Extensions_End}},
    {"get_integer64_indexed",
     GLVersion::GL3_2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::Extensions_End}},
    {"get_query_object_i64v",
     GLVersion::GL3_3,
     GLESVersion::NONE,
     GLContext::ARB_timer_query,
     {GLContext::ANGLE_timer_query, GLContext::EXT_disjoint_timer_query,
      GLContext::EXT_timer_query, GLContext::Extensions_End}},
    {
        "get_query_object_iv",
        GLVersion::GL2,
        GLESVersion::NONE,
        GLContext::Extension_None,
        {GLContext::Extensions_End}
        /*
         * XXX_get_query_object_iv only provide GetQueryObjectiv provided by
         * ARB_occlusion_query (added by OpenGL 2.0).
         */
    },
    {"gpu_shader4",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::EXT_gpu_shader4, GLContext::Extensions_End}},
    {"instanced_arrays",
     GLVersion::GL3_3,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_instanced_arrays, GLContext::NV_instanced_arrays,
      GLContext::ANGLE_instanced_arrays, GLContext::Extensions_End}},
    {
        "instanced_non_arrays",
        GLVersion::GL3_3,
        GLESVersion::ES3,
        GLContext::Extension_None,
        {GLContext::ARB_instanced_arrays, GLContext::Extensions_End}
        /* This is an expanded version of `instanced_arrays` that allows for all
         * enabled active attrib arrays to have non-zero divisors.
         * ANGLE_instanced_arrays and NV_instanced_arrays forbid this, but GLES3
         * has no such restriction.
         */
    },
    {"internalformat_query",
     GLVersion::GL4_2,
     GLESVersion::ES3,
     GLContext::ARB_internalformat_query,
     {GLContext::Extensions_End}},
    {"invalidate_framebuffer",
     GLVersion::GL4_3,
     GLESVersion::ES3,
     GLContext::ARB_invalidate_subdata,
     {GLContext::Extensions_End}},
    {"map_buffer_range",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::ARB_map_buffer_range,
     {GLContext::EXT_map_buffer_range, GLContext::Extensions_End}},
    {"multiview",
     GLVersion::NONE,
     GLESVersion::NONE,
     GLContext::Extension_None,
     {GLContext::ANGLE_multiview, GLContext::OVR_multiview2,
      GLContext::Extensions_End}},
    {
        "occlusion_query",
        GLVersion::GL2,
        GLESVersion::NONE,
        GLContext::Extension_None,
        {GLContext::Extensions_End}
        // XXX_occlusion_query depend on ARB_occlusion_query (added in
        // OpenGL 2.0)
    },
    {
        "occlusion_query_boolean",
        kGLCoreVersionForES3Compat,
        GLESVersion::ES3,
        GLContext::ARB_ES3_compatibility,
        {GLContext::EXT_occlusion_query_boolean, GLContext::Extensions_End}
        /*
         * XXX_occlusion_query_boolean provide ANY_SAMPLES_PASSED_CONSERVATIVE,
         * but EXT_occlusion_query_boolean is only a OpenGL ES extension. But
         * it is supported on desktop if ARB_ES3_compatibility because
         * EXT_occlusion_query_boolean (added in OpenGL ES 3.0).
         */
    },
    {
        "occlusion_query2",
        GLVersion::GL3_3,
        GLESVersion::ES3,
        GLContext::Extension_None,
        {GLContext::ARB_occlusion_query2, GLContext::ARB_ES3_compatibility,
         GLContext::EXT_occlusion_query_boolean, GLContext::Extensions_End}
        /*
         * XXX_occlusion_query2 (add in OpenGL 3.3) provide ANY_SAMPLES_PASSED,
         * which is provided by ARB_occlusion_query2,
         * EXT_occlusion_query_boolean (added in OpenGL ES 3.0) and
         * ARB_ES3_compatibility
         */
    },
    {"packed_depth_stencil",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::EXT_packed_depth_stencil, GLContext::OES_packed_depth_stencil,
      GLContext::Extensions_End}},
    {"prim_restart",
     GLVersion::GL3_1,
     GLESVersion::NONE,
     GLContext::Extension_None,
     {// GLContext::NV_primitive_restart, // Has different enum values.
      GLContext::Extensions_End}},
    {"prim_restart_fixed",
     kGLCoreVersionForES3Compat,
     GLESVersion::ES3,
     GLContext::ARB_ES3_compatibility,
     {GLContext::Extensions_End}},
    {"query_counter",
     GLVersion::GL3_3,
     GLESVersion::NONE,
     GLContext::ARB_timer_query,
     {GLContext::ANGLE_timer_query, GLContext::EXT_disjoint_timer_query,
      // EXT_timer_query does NOT support GL_TIMESTAMP retrieval with
      // QueryCounter.
      GLContext::Extensions_End}},
    {
        "query_objects",
        GLVersion::GL2,
        GLESVersion::ES3,
        GLContext::Extension_None,
        {GLContext::ANGLE_timer_query, GLContext::EXT_disjoint_timer_query,
         GLContext::EXT_occlusion_query_boolean, GLContext::Extensions_End}
        /*
         * XXX_query_objects only provide entry points commonly supported by
         * ARB_occlusion_query (added in OpenGL 2.0),
         * EXT_occlusion_query_boolean (added in OpenGL ES 3.0), and
         * ARB_timer_query (added in OpenGL 3.3)
         */
    },
    {"query_time_elapsed",
     GLVersion::GL3_3,
     GLESVersion::NONE,
     GLContext::ARB_timer_query,
     {GLContext::ANGLE_timer_query, GLContext::EXT_disjoint_timer_query,
      GLContext::EXT_timer_query, GLContext::Extensions_End}},
    {"read_buffer",
     GLVersion::GL2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::Extensions_End}},
    {"renderbuffer_color_float",
     GLVersion::GL3,
     GLESVersion::ES3_2,
     GLContext::Extension_None,
     {GLContext::ARB_texture_float, GLContext::EXT_color_buffer_float,
      GLContext::CHROMIUM_color_buffer_float_rgba, GLContext::Extensions_End}},
    {"renderbuffer_color_half_float",
     GLVersion::GL3,
     GLESVersion::ES3_2,
     GLContext::Extension_None,
     {GLContext::ARB_texture_float, GLContext::EXT_color_buffer_float,
      GLContext::EXT_color_buffer_half_float, GLContext::Extensions_End}},
    {"robust_buffer_access_behavior",
     GLVersion::NONE,
     GLESVersion::NONE,
     GLContext::Extension_None,
     {GLContext::ARB_robust_buffer_access_behavior,
      GLContext::KHR_robust_buffer_access_behavior, GLContext::Extensions_End}},
    {"robustness",
     GLVersion::NONE,
     GLESVersion::NONE,
     GLContext::Extension_None,
     {GLContext::ARB_robustness, GLContext::EXT_robustness,
      GLContext::KHR_robustness, GLContext::Extensions_End}},
    {"sRGB",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::ARB_framebuffer_sRGB,
     {GLContext::EXT_sRGB, GLContext::EXT_framebuffer_sRGB,
      GLContext::Extensions_End}},
    {"sampler_objects",
     GLVersion::GL3_3,
     GLESVersion::ES3,
     GLContext::ARB_sampler_objects,
     {GLContext::Extensions_End}},
    {"seamless_cube_map_opt_in",
     GLVersion::GL3_2,
     GLESVersion::NONE,
     GLContext::ARB_seamless_cube_map,
     {GLContext::Extensions_End}},
    {"shader_texture_lod",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_shader_texture_lod, GLContext::EXT_shader_texture_lod,
      GLContext::Extensions_End}},
    {// Do we have separate DRAW and READ framebuffer bind points?
     "split_framebuffer",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::ARB_framebuffer_object,
     {GLContext::ANGLE_framebuffer_blit,
      GLContext::APPLE_framebuffer_multisample, GLContext::EXT_framebuffer_blit,
      GLContext::NV_framebuffer_blit, GLContext::Extensions_End}},
    {"standard_derivatives",
     GLVersion::GL2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::OES_standard_derivatives, GLContext::Extensions_End}},
    {"sync",
     GLVersion::GL3_2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_sync, GLContext::APPLE_sync, GLContext::Extensions_End}},
    {"texture_3D",
     GLVersion::GL1_2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::OES_texture_3D, GLContext::Extensions_End}},
    {"texture_3D_compressed",
     GLVersion::GL1_3,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_texture_compression, GLContext::OES_texture_3D,
      GLContext::Extensions_End}},
    {"texture_3D_copy",
     GLVersion::GL1_2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::EXT_copy_texture, GLContext::OES_texture_3D,
      GLContext::Extensions_End}},
    {"texture_compression_bptc",
     GLVersion::GL4_2,
     GLESVersion::NONE,
     GLContext::Extension_None,
     {GLContext::ARB_texture_compression_bptc,
      GLContext::EXT_texture_compression_bptc, GLContext::Extensions_End}},
    {"texture_compression_rgtc",
     GLVersion::GL3,
     GLESVersion::NONE,
     GLContext::Extension_None,
     {GLContext::ARB_texture_compression_rgtc,
      GLContext::EXT_texture_compression_rgtc, GLContext::Extensions_End}},
    {"texture_float",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_texture_float, GLContext::OES_texture_float,
      GLContext::Extensions_End}},
    {"texture_float_linear",
     GLVersion::GL3_1,
     GLESVersion::NONE,
     GLContext::Extension_None,
     {GLContext::ARB_texture_float, GLContext::OES_texture_float_linear,
      GLContext::Extensions_End}},
    {
        "texture_half_float",
        GLVersion::GL3,
        GLESVersion::ES3,
        GLContext::Extension_None,
        {GLContext::ARB_half_float_pixel, GLContext::ARB_texture_float,
         GLContext::NV_half_float, GLContext::Extensions_End}
        /**
         * We are not including OES_texture_half_float in this feature, because:
         *   GL_HALF_FLOAT     = 0x140B
         *   GL_HALF_FLOAT_ARB = 0x140B == GL_HALF_FLOAT
         *   GL_HALF_FLOAT_NV  = 0x140B == GL_HALF_FLOAT
         *   GL_HALF_FLOAT_OES = 0x8D61 != GL_HALF_FLOAT
         * WebGL handles this specifically with an OES_texture_half_float check.
         */
    },
    {"texture_half_float_linear",
     GLVersion::GL3_1,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_half_float_pixel, GLContext::ARB_texture_float,
      GLContext::NV_half_float, GLContext::OES_texture_half_float_linear,
      GLContext::Extensions_End}},
    {"texture_non_power_of_two",
     GLVersion::GL2,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::ARB_texture_non_power_of_two, GLContext::OES_texture_npot,
      GLContext::Extensions_End}},
    {"texture_norm16",
     GLVersion::GL3_1,
     GLESVersion::ES3_1,
     GLContext::EXT_texture_norm16,
     {GLContext::Extensions_End}},
    {"texture_rg",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::ARB_texture_rg,
     {GLContext::Extensions_End}},
    {"texture_storage",
     GLVersion::GL4_2,
     GLESVersion::ES3,
     GLContext::ARB_texture_storage,
     {/*
       * Not including GL_EXT_texture_storage here because it
       * doesn't guarantee glTexStorage3D, which is required for
       * WebGL 2.
       */
      GLContext::Extensions_End}},
    {"texture_swizzle",
     GLVersion::GL3_3,
     GLESVersion::ES3,
     GLContext::ARB_texture_swizzle,
     {GLContext::Extensions_End}},
    {"transform_feedback2",
     GLVersion::GL4,
     GLESVersion::ES3,
     GLContext::ARB_transform_feedback2,
     {GLContext::NV_transform_feedback2, GLContext::Extensions_End}},
    {"uniform_buffer_object",
     GLVersion::GL3_1,
     GLESVersion::ES3,
     GLContext::ARB_uniform_buffer_object,
     {GLContext::Extensions_End}},
    {"uniform_matrix_nonsquare",
     GLVersion::GL2_1,
     GLESVersion::ES3,
     GLContext::Extension_None,
     {GLContext::Extensions_End}},
    {"vertex_array_object",
     GLVersion::GL3,
     GLESVersion::ES3,
     GLContext::ARB_vertex_array_object,  // ARB extension
     {GLContext::OES_vertex_array_object, GLContext::APPLE_vertex_array_object,
      GLContext::Extensions_End}}};

static inline const FeatureInfo& GetFeatureInfo(GLFeature feature) {
  static_assert(MOZ_ARRAY_LENGTH(sFeatureInfoArr) == size_t(GLFeature::EnumMax),
                "Mismatched lengths for sFeatureInfoInfos and GLFeature enums");

  MOZ_ASSERT(feature < GLFeature::EnumMax,
             "GLContext::GetFeatureInfoInfo : unknown <feature>");

  return sFeatureInfoArr[size_t(feature)];
}

static inline uint32_t ProfileVersionForFeature(GLFeature feature,
                                                ContextProfile profile) {
  MOZ_ASSERT(profile != ContextProfile::Unknown,
             "GLContext::ProfileVersionForFeature : unknown <profile>");

  const FeatureInfo& featureInfo = GetFeatureInfo(feature);

  if (profile == ContextProfile::OpenGLES)
    return (uint32_t)featureInfo.mOpenGLESVersion;

  return (uint32_t)featureInfo.mOpenGLVersion;
}

static bool IsFeaturePartOfProfileVersion(GLFeature feature,
                                          ContextProfile profile,
                                          unsigned int version) {
  unsigned int profileVersion = ProfileVersionForFeature(feature, profile);

  /**
   * if `profileVersion` is zero, it means that no version of the profile
   * added support for the feature.
   */
  return profileVersion && version >= profileVersion;
}

bool GLContext::IsFeatureProvidedByCoreSymbols(GLFeature feature) {
  if (IsFeaturePartOfProfileVersion(feature, mProfile, mVersion)) return true;

  if (IsExtensionSupported(
          GetFeatureInfo(feature).mARBExtensionWithoutARBSuffix))
    return true;

  return false;
}

const char* GLContext::GetFeatureName(GLFeature feature) {
  return GetFeatureInfo(feature).mName;
}

void GLContext::InitFeatures() {
  for (size_t featureId = 0; featureId < size_t(GLFeature::EnumMax);
       featureId++) {
    GLFeature feature = GLFeature(featureId);

    if (IsFeaturePartOfProfileVersion(feature, mProfile, mVersion)) {
      mAvailableFeatures[featureId] = true;
      continue;
    }

    mAvailableFeatures[featureId] = false;

    const FeatureInfo& featureInfo = GetFeatureInfo(feature);

    if (IsExtensionSupported(featureInfo.mARBExtensionWithoutARBSuffix)) {
      mAvailableFeatures[featureId] = true;
      continue;
    }

    for (size_t j = 0; true; j++) {
      MOZ_ASSERT(j < kMAX_EXTENSION_GROUP_SIZE,
                 "kMAX_EXTENSION_GROUP_SIZE too small");

      if (featureInfo.mExtensions[j] == GLContext::Extensions_End) break;

      if (IsExtensionSupported(featureInfo.mExtensions[j])) {
        mAvailableFeatures[featureId] = true;
        break;
      }
    }
  }

  if (ShouldDumpExts()) {
    for (size_t featureId = 0; featureId < size_t(GLFeature::EnumMax);
         featureId++) {
      GLFeature feature = GLFeature(featureId);
      printf_stderr("[%s] Feature::%s\n",
                    IsSupported(feature) ? "enabled" : "disabled",
                    GetFeatureName(feature));
    }
  }
}

void GLContext::MarkUnsupported(GLFeature feature) {
  mAvailableFeatures[size_t(feature)] = false;

  const FeatureInfo& featureInfo = GetFeatureInfo(feature);

  for (size_t i = 0; true; i++) {
    MOZ_ASSERT(i < kMAX_EXTENSION_GROUP_SIZE,
               "kMAX_EXTENSION_GROUP_SIZE too small");

    if (featureInfo.mExtensions[i] == GLContext::Extensions_End) break;

    MarkExtensionUnsupported(featureInfo.mExtensions[i]);
  }

  MOZ_ASSERT(!IsSupported(feature), "GLContext::MarkUnsupported has failed!");

  NS_WARNING(
      nsPrintfCString("%s marked as unsupported", GetFeatureName(feature))
          .get());
}

} /* namespace gl */
} /* namespace mozilla */
