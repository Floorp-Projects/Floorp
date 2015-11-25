/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionTextureFloat::WebGLExtensionTextureFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    MOZ_ASSERT(IsSupported(webgl));

    auto& fua = webgl->mFormatUsage;
    gl::GLContext* gl = webgl->GL();

    webgl::PackingInfo pi;
    webgl::DriverUnpackInfo dui;
    const GLint* swizzle = nullptr;

    const auto fnAdd = [&fua, &pi, &dui, &swizzle](webgl::EffectiveFormat effFormat)
    {
        auto usage = fua->EditUsage(effFormat);
        usage->textureSwizzleRGBA = swizzle;
        fua->AddTexUnpack(usage, pi, dui);

        fua->AllowUnsizedTexFormat(pi, usage);
    };

    const bool needSizedInternal = !gl->IsGLES();
    MOZ_ASSERT_IF(needSizedInternal, gl->IsSupported(gl::GLFeature::texture_swizzle));

    ////////////////

    pi = {LOCAL_GL_RGBA, LOCAL_GL_FLOAT};
    dui = {pi.format, pi.format, pi.type};
    swizzle = nullptr;
    if (needSizedInternal) {
        dui.internalFormat = LOCAL_GL_RGBA32F;
    }
    fnAdd(webgl::EffectiveFormat::RGBA32F);

    //////

    pi = {LOCAL_GL_RGB, LOCAL_GL_FLOAT};
    dui = {pi.format, pi.format, pi.type};
    swizzle = nullptr;
    if (needSizedInternal) {
        dui.internalFormat = LOCAL_GL_RGB32F;
    }
    fnAdd(webgl::EffectiveFormat::RGB32F);

    //////

    pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_FLOAT};
    dui = {pi.format, pi.format, pi.type};
    swizzle = nullptr;
    if (needSizedInternal) {
        dui = {LOCAL_GL_R32F, LOCAL_GL_RED, LOCAL_GL_FLOAT};
        swizzle = webgl::FormatUsageInfo::kLuminanceSwizzleRGBA;
    }
    fnAdd(webgl::EffectiveFormat::Luminance32F);

    //////

    pi = {LOCAL_GL_ALPHA, LOCAL_GL_FLOAT};
    dui = {pi.format, pi.format, pi.type};
    swizzle = nullptr;
    if (needSizedInternal) {
        dui = {LOCAL_GL_R32F, LOCAL_GL_RED, LOCAL_GL_FLOAT};
        swizzle = webgl::FormatUsageInfo::kAlphaSwizzleRGBA;
    }
    fnAdd(webgl::EffectiveFormat::Alpha32F);

    //////

    pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_FLOAT};
    dui = {pi.format, pi.format, pi.type};
    swizzle = nullptr;
    if (needSizedInternal) {
        dui = {LOCAL_GL_RG32F, LOCAL_GL_RG, LOCAL_GL_FLOAT};
        swizzle = webgl::FormatUsageInfo::kLumAlphaSwizzleRGBA;
    }
    fnAdd(webgl::EffectiveFormat::Luminance32FAlpha32F);
}

WebGLExtensionTextureFloat::~WebGLExtensionTextureFloat()
{
}

bool
WebGLExtensionTextureFloat::IsSupported(const WebGLContext* webgl)
{
    gl::GLContext* gl = webgl->GL();

    if (!gl->IsSupported(gl::GLFeature::texture_float))
        return false;

    const bool needSizedInternal = !gl->IsGLES();
    const bool hasSwizzle = gl->IsSupported(gl::GLFeature::texture_swizzle);

    if (needSizedInternal && !hasSwizzle)
        return false;

    return true;
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureFloat, OES_texture_float)

} // namespace mozilla
