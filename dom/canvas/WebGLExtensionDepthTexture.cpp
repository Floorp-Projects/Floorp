/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionDepthTexture::WebGLExtensionDepthTexture(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    auto& fua = webgl->mFormatUsage;

    const auto fnAdd = [&fua](webgl::EffectiveFormat effFormat, GLenum unpackFormat,
                              GLenum unpackType)
    {
        auto usage = fua->EditUsage(effFormat);
        usage->isFilterable = true;
        usage->SetRenderable();

        const webgl::PackingInfo pi = {unpackFormat, unpackType};
        const webgl::DriverUnpackInfo dui = {unpackFormat, unpackFormat, unpackType};
        fua->AddTexUnpack(usage, pi, dui);

        fua->AllowUnsizedTexFormat(pi, usage);
    };

    fnAdd(webgl::EffectiveFormat::DEPTH_COMPONENT16, LOCAL_GL_DEPTH_COMPONENT,
          LOCAL_GL_UNSIGNED_SHORT);

    fnAdd(webgl::EffectiveFormat::DEPTH_COMPONENT24, LOCAL_GL_DEPTH_COMPONENT,
          LOCAL_GL_UNSIGNED_INT);

    fnAdd(webgl::EffectiveFormat::DEPTH24_STENCIL8, LOCAL_GL_DEPTH_STENCIL,
          LOCAL_GL_UNSIGNED_INT_24_8);
}

WebGLExtensionDepthTexture::~WebGLExtensionDepthTexture()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionDepthTexture, WEBGL_depth_texture)

} // namespace mozilla
