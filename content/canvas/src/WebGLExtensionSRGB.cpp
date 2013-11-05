/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "GLContext.h"

using namespace mozilla;

WebGLExtensionSRGB::WebGLExtensionSRGB(WebGLContext* context)
    : WebGLExtensionBase(context)
{
    MOZ_ASSERT(IsSupported(context), "should not construct WebGLExtensionSRGB: "
                                     "sRGB is unsupported.");
    gl::GLContext* gl = context->GL();
    if (!gl->IsGLES()) {
        // Desktop OpenGL requires the following to be enabled to support
        // sRGB operations on framebuffers
        gl->MakeCurrent();
        gl->fEnable(LOCAL_GL_FRAMEBUFFER_SRGB_EXT);
    }
}

WebGLExtensionSRGB::~WebGLExtensionSRGB()
{
}

bool
WebGLExtensionSRGB::IsSupported(const WebGLContext* context)
{
    gl::GLContext* gl = context->GL();

    return gl->IsSupported(gl::GLFeature::sRGB);
}


IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionSRGB)
