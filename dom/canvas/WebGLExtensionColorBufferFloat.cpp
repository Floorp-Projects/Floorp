/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

using mozilla::webgl::EffectiveFormat;

WebGLExtensionColorBufferFloat::WebGLExtensionColorBufferFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");

    webgl::FormatUsageAuthority* authority = webgl->mFormatUsage.get();

    auto updateUsage = [authority](EffectiveFormat effectiveFormat) {
        webgl::FormatUsageInfo* usage = authority->GetUsage(effectiveFormat);
        MOZ_ASSERT(usage);
        usage->asRenderbuffer = usage->isRenderable = true;
    };

    // Ensure require formats are initialized.
    WebGLExtensionTextureFloat::InitWebGLFormats(authority);

    // Update usage to allow asRenderbuffer and isRenderable
    updateUsage(EffectiveFormat::RGBA32F);
    updateUsage(EffectiveFormat::RGB32F);
    updateUsage(EffectiveFormat::Luminance32FAlpha32F);
    updateUsage(EffectiveFormat::Luminance32F);
    updateUsage(EffectiveFormat::Alpha32F);
}

WebGLExtensionColorBufferFloat::~WebGLExtensionColorBufferFloat()
{
}

bool
WebGLExtensionColorBufferFloat::IsSupported(const WebGLContext* webgl)
{
    gl::GLContext* gl = webgl->GL();

    // ANGLE supports this, but doesn't have a way to advertize its support,
    // since it's compliant with WEBGL_color_buffer_float's clamping, but not
    // EXT_color_buffer_float.
    return gl->IsSupported(gl::GLFeature::renderbuffer_color_float) ||
           gl->IsANGLE();
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionColorBufferFloat, WEBGL_color_buffer_float)

} // namespace mozilla
