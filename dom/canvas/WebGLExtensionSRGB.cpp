/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

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

    auto& fua = webgl->mFormatUsage;

    RefPtr<gl::GLContext> gl_ = gl; // Bug 1201275
    const auto fnAdd = [&fua, &gl_](webgl::EffectiveFormat effFormat, GLenum format,
                                    GLenum desktopUnpackFormat)
    {
        auto usage = fua->EditUsage(effFormat);
        usage->isFilterable = true;

        webgl::DriverUnpackInfo dui = {format, format, LOCAL_GL_UNSIGNED_BYTE};
        const auto pi = dui.ToPacking();

        if (!gl_->IsGLES())
            dui.unpackFormat = desktopUnpackFormat;

        fua->AddTexUnpack(usage, pi, dui);

        fua->AllowUnsizedTexFormat(pi, usage);
    };

    fnAdd(webgl::EffectiveFormat::SRGB8, LOCAL_GL_SRGB, LOCAL_GL_RGB);
    fnAdd(webgl::EffectiveFormat::SRGB8_ALPHA8, LOCAL_GL_SRGB_ALPHA, LOCAL_GL_RGBA);

    auto usage = fua->EditUsage(webgl::EffectiveFormat::SRGB8_ALPHA8);
    usage->isRenderable = true;
    fua->AllowRBFormat(LOCAL_GL_SRGB8_ALPHA8, usage);
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
