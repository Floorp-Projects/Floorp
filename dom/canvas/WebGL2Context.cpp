/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::gl;

// -----------------------------------------------------------------------------
// CONSTRUCTOR & DESTRUCTOR

WebGL2Context::WebGL2Context()
    : WebGLContext()
{
    MOZ_ASSERT(IsSupported(), "not supposed to create a WebGL2Context"
                              "context when not supported");
}

WebGL2Context::~WebGL2Context()
{

}


// -----------------------------------------------------------------------------
// STATIC FUNCTIONS

bool
WebGL2Context::IsSupported()
{
    return Preferences::GetBool("webgl.enable-prototype-webgl2", false);
}

WebGL2Context*
WebGL2Context::Create()
{
    return new WebGL2Context();
}


// -----------------------------------------------------------------------------
// IMPLEMENT nsWrapperCache

JSObject*
WebGL2Context::WrapObject(JSContext *cx)
{
    return dom::WebGL2RenderingContextBinding::Wrap(cx, this);
}


// -----------------------------------------------------------------------------
// WebGL 2 initialisation

bool
WebGLContext::InitWebGL2()
{
    MOZ_ASSERT(IsWebGL2(), "WebGLContext is not a WebGL 2 context!");

    const WebGLExtensionID sExtensionNativelySupportedArr[] = {
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
    const GLFeature sFeatureRequiredArr[] = {
        GLFeature::instanced_non_arrays,
        GLFeature::transform_feedback2
    };

    // check WebGL extensions that are supposed to be natively supported
    for (size_t i = 0; i < size_t(MOZ_ARRAY_LENGTH(sExtensionNativelySupportedArr)); i++)
    {
        WebGLExtensionID extension = sExtensionNativelySupportedArr[i];

        if (!IsExtensionSupported(extension)) {
            GenerateWarning("WebGL 2 requires %s!", GetExtensionString(extension));
            return false;
        }
    }

    // check required OpenGL extensions
    if (!gl->IsExtensionSupported(GLContext::EXT_gpu_shader4)) {
        GenerateWarning("WebGL 2 requires GL_EXT_gpu_shader4!");
        return false;
    }

    // check OpenGL features
    if (!gl->IsSupported(GLFeature::occlusion_query) &&
        !gl->IsSupported(GLFeature::occlusion_query_boolean))
    {
        /*
         * on desktop, we fake occlusion_query_boolean with occlusion_query if
         * necessary. See WebGLContextAsyncQueries.cpp.
         */
        GenerateWarning("WebGL 2 requires occlusion queries!");
        return false;
    }

    for (size_t i = 0; i < size_t(MOZ_ARRAY_LENGTH(sFeatureRequiredArr)); i++)
    {
        if (!gl->IsSupported(sFeatureRequiredArr[i])) {
            GenerateWarning("WebGL 2 requires GLFeature::%s!", GLContext::GetFeatureName(sFeatureRequiredArr[i]));
            return false;
        }
    }

    // ok WebGL 2 is compatible, we can enable natively supported extensions.
    for (size_t i = 0; i < size_t(MOZ_ARRAY_LENGTH(sExtensionNativelySupportedArr)); i++) {
        EnableExtension(sExtensionNativelySupportedArr[i]);

        MOZ_ASSERT(IsExtensionEnabled(sExtensionNativelySupportedArr[i]));
    }

    // we initialise WebGL 2 related stuff.
    gl->GetUIntegerv(LOCAL_GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &mGLMaxTransformFeedbackSeparateAttribs);

    return true;
}
