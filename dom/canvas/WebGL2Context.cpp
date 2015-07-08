/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "WebGLBuffer.h"
#include "WebGLFormats.h"
#include "WebGLTransformFeedback.h"

namespace mozilla {

WebGL2Context::WebGL2Context()
    : WebGLContext()
{
    MOZ_ASSERT(IsSupported(), "not supposed to create a WebGL2Context"
                              "context when not supported");

    mFormatUsage = Move(webgl::FormatUsageAuthority::CreateForWebGL2());
}

WebGL2Context::~WebGL2Context()
{

}

/*static*/ bool
WebGL2Context::IsSupported()
{
    return Preferences::GetBool("webgl.enable-prototype-webgl2", false);
}

/*static*/ WebGL2Context*
WebGL2Context::Create()
{
    return new WebGL2Context();
}

JSObject*
WebGL2Context::WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto)
{
    return dom::WebGL2RenderingContextBinding::Wrap(cx, this, aGivenProto);
}

////////////////////////////////////////////////////////////////////////////////
// WebGL 2 initialisation

// These WebGL 1 extensions are natively supported by WebGL 2.
static const WebGLExtensionID kNativelySupportedExtensions[] = {
    WebGLExtensionID::ANGLE_instanced_arrays,
    WebGLExtensionID::EXT_blend_minmax,
    WebGLExtensionID::EXT_sRGB,
    WebGLExtensionID::OES_element_index_uint,
    WebGLExtensionID::OES_standard_derivatives,
    WebGLExtensionID::OES_texture_float,
    WebGLExtensionID::OES_texture_float_linear,
    WebGLExtensionID::OES_texture_half_float,
    WebGLExtensionID::OES_texture_half_float_linear,
    WebGLExtensionID::OES_vertex_array_object,
    WebGLExtensionID::WEBGL_depth_texture,
    WebGLExtensionID::WEBGL_draw_buffers
};

static const gl::GLFeature kRequiredFeatures[] = {
    gl::GLFeature::blend_minmax,
    gl::GLFeature::clear_buffers,
    gl::GLFeature::copy_buffer,
    gl::GLFeature::depth_texture,
    gl::GLFeature::draw_instanced,
    gl::GLFeature::draw_range_elements,
    gl::GLFeature::element_index_uint,
    gl::GLFeature::frag_color_float,
    gl::GLFeature::frag_depth,
    gl::GLFeature::framebuffer_blit,
    gl::GLFeature::framebuffer_multisample,
    gl::GLFeature::get_integer_indexed,
    gl::GLFeature::get_integer64_indexed,
    gl::GLFeature::gpu_shader4,
    gl::GLFeature::instanced_arrays,
    gl::GLFeature::instanced_non_arrays,
    gl::GLFeature::map_buffer_range,
    gl::GLFeature::occlusion_query2,
    gl::GLFeature::packed_depth_stencil,
    gl::GLFeature::query_objects,
    gl::GLFeature::renderbuffer_color_float,
    gl::GLFeature::renderbuffer_color_half_float,
    gl::GLFeature::sRGB_framebuffer,
    gl::GLFeature::sRGB_texture,
    gl::GLFeature::sampler_objects,
    gl::GLFeature::standard_derivatives,
    gl::GLFeature::texture_3D,
    gl::GLFeature::texture_3D_compressed,
    gl::GLFeature::texture_3D_copy,
    gl::GLFeature::texture_float,
    gl::GLFeature::texture_float_linear,
    gl::GLFeature::texture_half_float,
    gl::GLFeature::texture_half_float_linear,
    gl::GLFeature::texture_non_power_of_two,
    gl::GLFeature::texture_storage,
    gl::GLFeature::transform_feedback2,
    gl::GLFeature::uniform_buffer_object,
    gl::GLFeature::uniform_matrix_nonsquare,
    gl::GLFeature::vertex_array_object
};

bool
WebGLContext::InitWebGL2()
{
    MOZ_ASSERT(IsWebGL2(), "WebGLContext is not a WebGL 2 context!");

    // check OpenGL features
    if (!gl->IsSupported(gl::GLFeature::occlusion_query) &&
        !gl->IsSupported(gl::GLFeature::occlusion_query_boolean))
    {
        // On desktop, we fake occlusion_query_boolean with occlusion_query if
        // necessary. (See WebGL2ContextQueries.cpp)
        GenerateWarning("WebGL 2 unavailable. Requires occlusion queries.");
        return false;
    }

    std::vector<gl::GLFeature> missingList;

    for (size_t i = 0; i < ArrayLength(kRequiredFeatures); i++) {
        if (!gl->IsSupported(kRequiredFeatures[i]))
            missingList.push_back(kRequiredFeatures[i]);
    }

#ifdef XP_MACOSX
    // On OSX, GL core profile is used. This requires texture swizzle
    // support to emulate legacy texture formats: ALPHA, LUMINANCE,
    // and LUMINANCE_ALPHA.
    if (!gl->IsSupported(gl::GLFeature::texture_swizzle))
        missingList.push_back(gl::GLFeature::texture_swizzle);
#endif

    if (missingList.size()) {
        nsAutoCString exts;
        for (auto itr = missingList.begin(); itr != missingList.end(); ++itr) {
            exts.AppendLiteral("\n  ");
            exts.Append(gl::GLContext::GetFeatureName(*itr));
        }
        GenerateWarning("WebGL 2 unavailable. The following required features are"
                        " unavailible: %s", exts.BeginReading());
        return false;
    }

    // ok WebGL 2 is compatible, we can enable natively supported extensions.
    for (size_t i = 0; i < ArrayLength(kNativelySupportedExtensions); i++) {
        EnableExtension(kNativelySupportedExtensions[i]);

        MOZ_ASSERT(IsExtensionEnabled(kNativelySupportedExtensions[i]));
    }

    // we initialise WebGL 2 related stuff.
    gl->GetUIntegerv(LOCAL_GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
                     &mGLMaxTransformFeedbackSeparateAttribs);
    gl->GetUIntegerv(LOCAL_GL_MAX_UNIFORM_BUFFER_BINDINGS,
                     &mGLMaxUniformBufferBindings);

    mBoundTransformFeedbackBuffers.SetLength(mGLMaxTransformFeedbackSeparateAttribs);
    mBoundUniformBuffers.SetLength(mGLMaxUniformBufferBindings);

    mDefaultTransformFeedback = new WebGLTransformFeedback(this, 0);
    mBoundTransformFeedback = mDefaultTransformFeedback;

    mBypassShaderValidation = true;

    return true;
}

} // namespace mozilla
