/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"

#include "GLContext.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

WebGLExtensionBlendMinMax::WebGLExtensionBlendMinMax(WebGLContext* context)
    : WebGLExtensionBase(context)
{
}

WebGLExtensionBlendMinMax::~WebGLExtensionBlendMinMax()
{
}

bool WebGLExtensionBlendMinMax::IsSupported(const WebGLContext* context)
{
    return context->GL()->IsSupported(gl::GLFeature::blend_minmax);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionBlendMinMax)
