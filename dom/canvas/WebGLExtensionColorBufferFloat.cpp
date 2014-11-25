/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

using namespace mozilla;

WebGLExtensionColorBufferFloat::WebGLExtensionColorBufferFloat(WebGLContext* context)
    : WebGLExtensionBase(context)
{
    MOZ_ASSERT(IsSupported(context));
}

WebGLExtensionColorBufferFloat::~WebGLExtensionColorBufferFloat()
{
}

bool
WebGLExtensionColorBufferFloat::IsSupported(const WebGLContext* context)
{
    gl::GLContext* gl = context->GL();

    // ANGLE supports this, but doesn't have a way to advertize its support,
    // since it's compliant with WEBGL_color_buffer_float's clamping, but not
    // EXT_color_buffer_float.
    return gl->IsSupported(gl::GLFeature::renderbuffer_color_float) ||
           gl->IsANGLE();
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionColorBufferFloat)
