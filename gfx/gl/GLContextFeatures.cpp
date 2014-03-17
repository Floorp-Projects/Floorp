/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "GLContext.h"
#include "nsPrintfCString.h"

#ifdef XP_MACOSX
#include "nsCocoaFeatures.h"
#endif

namespace mozilla {
namespace gl {

const size_t kMAX_EXTENSION_GROUP_SIZE = 5;

// ARB_ES2_compatibility is natively supported in OpenGL 4.1.
static const unsigned int kGLCoreVersionForES2Compat = 410;

// ARB_ES3_compatibility is natively supported in OpenGL 4.3.
static const unsigned int kGLCoreVersionForES3Compat = 430;

struct FeatureInfo
{
    const char* mName;
    unsigned int mOpenGLVersion;
    unsigned int mOpenGLESVersion;
    GLContext::GLExtensions mExtensions[kMAX_EXTENSION_GROUP_SIZE];
};

static const FeatureInfo sFeatureInfoArr[] = {
    {
        "bind_buffer_offset",
        0,   // OpenGL version
        0,   // OpenGL ES version
        {
            GLContext::EXT_transform_feedback,
            GLContext::NV_transform_feedback,
            GLContext::Extensions_End
        }
    },
    {
        "blend_minmax",
        200, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::EXT_blend_minmax,
            GLContext::Extensions_End
        }
    },
    {
        "depth_texture",
        200, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_depth_texture,
            GLContext::OES_depth_texture,
            // Intentionally avoid putting ANGLE_depth_texture here,
            // it does not offer quite the same functionality.
            GLContext::Extensions_End
        }
    },
    {
        "draw_buffers",
        200, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_draw_buffers,
            GLContext::EXT_draw_buffers,
            GLContext::Extensions_End
        }
    },
    {
        "draw_instanced",
        310, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_draw_instanced,
            GLContext::EXT_draw_instanced,
            GLContext::NV_draw_instanced,
            GLContext::ANGLE_instanced_arrays,
            GLContext::Extensions_End
        }
    },
    {
        "draw_range_elements",
        120, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::EXT_draw_range_elements,
            GLContext::Extensions_End
        }
    },
    {
        "element_index_uint",
        200, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::OES_element_index_uint,
            GLContext::Extensions_End
        }
    },
    {
        "ES2_compatibility",
        kGLCoreVersionForES2Compat,
        200, // OpenGL ES version
        {
            GLContext::ARB_ES2_compatibility,
            GLContext::Extensions_End
        }
    },
    {
        "ES3_compatibility",
        kGLCoreVersionForES3Compat,
        300, // OpenGL ES version
        {
            GLContext::ARB_ES3_compatibility,
            GLContext::Extensions_End
        }
    },
    {
        // Removes clamping for float color outputs from frag shaders.
        "frag_color_float",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_color_buffer_float,
            GLContext::EXT_color_buffer_float,
            GLContext::EXT_color_buffer_half_float,
            GLContext::Extensions_End
        }
    },
    {
        "frag_depth",
        200, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::EXT_frag_depth,
            GLContext::Extensions_End
        }
    },
    {
        "framebuffer_blit",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::EXT_framebuffer_blit,
            GLContext::ANGLE_framebuffer_blit,
            GLContext::Extensions_End
        }
    },
    {
        "framebuffer_multisample",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::EXT_framebuffer_multisample,
            GLContext::ANGLE_framebuffer_multisample,
            GLContext::Extensions_End
        }
    },
    {
        "framebuffer_object",
        300, // OpenGL version
        200, // OpenGL ES version
        {
            GLContext::ARB_framebuffer_object,
            GLContext::EXT_framebuffer_object,
            GLContext::Extensions_End
        }
    },
    {
        "get_query_object_iv",
        200, // OpenGL version
        0,   // OpenGL ES version
        {
            GLContext::Extensions_End
        }
        /*
         * XXX_get_query_object_iv only provide GetQueryObjectiv provided by
         * ARB_occlusion_query (added by OpenGL 2.0).
         */
    },
    {
        "instanced_arrays",
        330, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_instanced_arrays,
            GLContext::NV_instanced_arrays,
            GLContext::ANGLE_instanced_arrays,
            GLContext::Extensions_End
        }
    },
    {
        "instanced_non_arrays",
        330, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_instanced_arrays,
            GLContext::Extensions_End
        }
        /* This is an expanded version of `instanced_arrays` that allows for all
         * enabled active attrib arrays to have non-zero divisors.
         * ANGLE_instanced_arrays and NV_instanced_arrays forbid this, but GLES3
         * has no such restriction.
         */
    },
    {
        "occlusion_query",
        200, // OpenGL version
        0,   // OpenGL ES version
        {
            GLContext::Extensions_End
        }
        // XXX_occlusion_query depend on ARB_occlusion_query (added in OpenGL 2.0)
    },
    {
        "occlusion_query_boolean",
        kGLCoreVersionForES3Compat,
        300, // OpenGL ES version
        {
            GLContext::ARB_ES3_compatibility,
            GLContext::EXT_occlusion_query_boolean,
            GLContext::Extensions_End
        }
        /*
         * XXX_occlusion_query_boolean provide ANY_SAMPLES_PASSED_CONSERVATIVE,
         * but EXT_occlusion_query_boolean is only a OpenGL ES extension. But
         * it is supported on desktop if ARB_ES3_compatibility because
         * EXT_occlusion_query_boolean (added in OpenGL ES 3.0).
         */
    },
    {
        "occlusion_query2",
        330, // = min(330, kGLCoreVersionForES3Compat),
        300, // OpenGL ES version
        {
            GLContext::ARB_occlusion_query2,
            GLContext::ARB_ES3_compatibility,
            GLContext::EXT_occlusion_query_boolean,
            GLContext::Extensions_End
        }
        /*
         * XXX_occlusion_query2 (add in OpenGL 3.3) provide ANY_SAMPLES_PASSED,
         * which is provided by ARB_occlusion_query2, EXT_occlusion_query_boolean
         * (added in OpenGL ES 3.0) and ARB_ES3_compatibility
         */
    },
    {
        "packed_depth_stencil",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::EXT_packed_depth_stencil,
            GLContext::OES_packed_depth_stencil,
            GLContext::Extensions_End
        }
    },
    {
        "query_objects",
        200, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::EXT_occlusion_query_boolean,
            GLContext::Extensions_End
        }
        /*
         * XXX_query_objects only provide entry points commonly supported by
         * ARB_occlusion_query (added in OpenGL 2.0) and EXT_occlusion_query_boolean
         * (added in OpenGL ES 3.0)
         */
    },
    {
        "renderbuffer_float",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_texture_float,
            GLContext::EXT_color_buffer_float,
            GLContext::Extensions_End
        }
    },
    {
        "renderbuffer_half_float",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_texture_float,
            GLContext::EXT_color_buffer_half_float,
            GLContext::Extensions_End
        }
    },
    {
        "robustness",
        0,   // OpenGL version
        0,   // OpenGL ES version
        {
            GLContext::ARB_robustness,
            GLContext::EXT_robustness,
            GLContext::Extensions_End
        }
    },
    {
        "sRGB",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::EXT_sRGB,
            GLContext::Extensions_End
        }
    },
    {
        "standard_derivatives",
        200, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::OES_standard_derivatives,
            GLContext::Extensions_End
        }
    },
    {
        "texture_float",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_texture_float,
            GLContext::OES_texture_float,
            GLContext::Extensions_End
        }
    },
    {
        "texture_float_linear",
        310, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_texture_float,
            GLContext::OES_texture_float_linear,
            GLContext::Extensions_End
        }
    },
    {
        "texture_half_float",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_half_float_pixel,
            GLContext::ARB_texture_float,
            GLContext::NV_half_float,
            GLContext::Extensions_End
        }
        /**
         * We are not including OES_texture_half_float in this feature, because:
         *   GL_HALF_FLOAT     = 0x140B
         *   GL_HALF_FLOAT_ARB = 0x140B == GL_HALF_FLOAT
         *   GL_HALF_FLOAT_NV  = 0x140B == GL_HALF_FLOAT
         *   GL_HALF_FLOAT_OES = 0x8D61 != GL_HALF_FLOAT
         * WebGL handles this specifically with an OES_texture_half_float check.
         */
    },
    {
        "texture_half_float_linear",
        310, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_half_float_pixel,
            GLContext::ARB_texture_float,
            GLContext::NV_half_float,
            GLContext::OES_texture_half_float_linear,
            GLContext::Extensions_End
        }
    },
    {
        "texture_non_power_of_two",
        200, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_texture_non_power_of_two,
            GLContext::OES_texture_npot,
            GLContext::Extensions_End
        }
    },
    {
        "transform_feedback",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::EXT_transform_feedback,
            GLContext::NV_transform_feedback,
            GLContext::Extensions_End
        }
    },
    {
        "vertex_array_object",
        300, // OpenGL version
        300, // OpenGL ES version
        {
            GLContext::ARB_vertex_array_object,
            GLContext::OES_vertex_array_object,
            GLContext::APPLE_vertex_array_object,
            GLContext::Extensions_End
        }
    }
};

static inline const FeatureInfo&
GetFeatureInfo(GLFeature feature)
{
    static_assert(MOZ_ARRAY_LENGTH(sFeatureInfoArr) == size_t(GLFeature::EnumMax),
                  "Mismatched lengths for sFeatureInfoInfos and GLFeature enums");

    MOZ_ASSERT(feature < GLFeature::EnumMax,
               "GLContext::GetFeatureInfoInfo : unknown <feature>");

    return sFeatureInfoArr[size_t(feature)];
}

static inline uint32_t
ProfileVersionForFeature(GLFeature feature, ContextProfile profile)
{
    MOZ_ASSERT(profile != ContextProfile::Unknown,
               "GLContext::ProfileVersionForFeature : unknown <profile>");

    const FeatureInfo& featureInfo = GetFeatureInfo(feature);

    if (profile == ContextProfile::OpenGLES) {
        return featureInfo.mOpenGLESVersion;
    }

    return featureInfo.mOpenGLVersion;
}

static inline bool
IsFeatureIsPartOfProfileVersion(GLFeature feature,
                                ContextProfile profile, unsigned int version)
{
    unsigned int profileVersion = ProfileVersionForFeature(feature, profile);

    /**
     * if `profileVersion` is zero, it means that no version of the profile
     * added support for the feature.
     */
    return profileVersion && version >= profileVersion;
}

const char*
GLContext::GetFeatureName(GLFeature feature)
{
    return GetFeatureInfo(feature).mName;
}

static bool
CanReadSRGBFromFBOTexture(GLContext* gl)
{
    if (!gl->WorkAroundDriverBugs())
        return true;

#ifdef XP_MACOSX
    // Bug 843668:
    // MacOSX 10.6 reports to support EXT_framebuffer_sRGB and
    // EXT_texture_sRGB but fails to convert from sRGB to linear
    // when writing to an sRGB texture attached to an FBO.
    if (!nsCocoaFeatures::OnLionOrLater()) {
        return false;
    }
#endif // XP_MACOSX
    return true;
}

void
GLContext::InitFeatures()
{
    for (size_t feature_index = 0; feature_index < size_t(GLFeature::EnumMax); feature_index++)
    {
        GLFeature feature = GLFeature(feature_index);

        if (IsFeatureIsPartOfProfileVersion(feature, mProfile, mVersion)) {
            mAvailableFeatures[feature_index] = true;
            continue;
        }

        mAvailableFeatures[feature_index] = false;

        const FeatureInfo& featureInfo = GetFeatureInfo(feature);

        for (size_t j = 0; true; j++)
        {
            MOZ_ASSERT(j < kMAX_EXTENSION_GROUP_SIZE, "kMAX_EXTENSION_GROUP_SIZE too small");

            if (featureInfo.mExtensions[j] == GLContext::Extensions_End) {
                break;
            }

            if (IsExtensionSupported(featureInfo.mExtensions[j])) {
                mAvailableFeatures[feature_index] = true;
                break;
            }
        }
    }

    // Bug 843668: Work around limitation of the feature system.
    // For sRGB support under OpenGL to match OpenGL ES spec, check for both
    // EXT_texture_sRGB and EXT_framebuffer_sRGB is required.
    const bool aresRGBExtensionsAvailable =
        IsExtensionSupported(EXT_texture_sRGB) &&
        (IsExtensionSupported(ARB_framebuffer_sRGB) ||
         IsExtensionSupported(EXT_framebuffer_sRGB));

    mAvailableFeatures[size_t(GLFeature::sRGB)] =
        aresRGBExtensionsAvailable &&
        CanReadSRGBFromFBOTexture(this);
}

void
GLContext::MarkUnsupported(GLFeature feature)
{
    mAvailableFeatures[size_t(feature)] = false;

    const FeatureInfo& featureInfo = GetFeatureInfo(feature);

    for (size_t i = 0; true; i++)
    {
        MOZ_ASSERT(i < kMAX_EXTENSION_GROUP_SIZE, "kMAX_EXTENSION_GROUP_SIZE too small");

        if (featureInfo.mExtensions[i] == GLContext::Extensions_End) {
            break;
        }

        MarkExtensionUnsupported(featureInfo.mExtensions[i]);
    }

    MOZ_ASSERT(!IsSupported(feature), "GLContext::MarkUnsupported has failed!");

    NS_WARNING(nsPrintfCString("%s marked as unsupported", GetFeatureName(feature)).get());
}

} /* namespace gl */
} /* namespace mozilla */
