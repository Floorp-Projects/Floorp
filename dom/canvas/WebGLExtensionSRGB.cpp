/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionSRGB::WebGLExtensionSRGB(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");

    gl::GLContext* gl = webgl->GL();
    if (!gl->IsGLES()) {
        // Desktop OpenGL requires the following to be enabled in order to
        // support sRGB operations on framebuffers.
        gl->MakeCurrent();
        gl->fEnable(LOCAL_GL_FRAMEBUFFER_SRGB_EXT);
    }
}

WebGLExtensionSRGB::~WebGLExtensionSRGB()
{
}

bool
WebGLExtensionSRGB::IsSupported(const WebGLContext* webgl)
{
    gl::GLContext* gl = webgl->GL();

    return gl->IsSupported(gl::GLFeature::sRGB_framebuffer) &&
           gl->IsSupported(gl::GLFeature::sRGB_texture);
}


IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionSRGB, EXT_sRGB)

} // namespace mozilla
