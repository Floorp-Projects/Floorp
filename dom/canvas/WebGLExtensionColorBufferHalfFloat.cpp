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

WebGLExtensionColorBufferHalfFloat::WebGLExtensionColorBufferHalfFloat(WebGLContext* webgl)
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
    WebGLExtensionTextureHalfFloat::InitWebGLFormats(authority);

    // Update usage to allow asRenderbuffer and isRenderable
    updateUsage(EffectiveFormat::RGBA16F);
    updateUsage(EffectiveFormat::RGB16F);
    updateUsage(EffectiveFormat::Luminance16FAlpha16F);
    updateUsage(EffectiveFormat::Luminance16F);
    updateUsage(EffectiveFormat::Alpha16F);
}

WebGLExtensionColorBufferHalfFloat::~WebGLExtensionColorBufferHalfFloat()
{
}

bool
WebGLExtensionColorBufferHalfFloat::IsSupported(const WebGLContext* webgl)
{
    gl::GLContext* gl = webgl->GL();

    // ANGLE doesn't support ReadPixels from a RGBA16F with RGBA/FLOAT.
    return gl->IsSupported(gl::GLFeature::renderbuffer_color_half_float) ||
           gl->IsANGLE();
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionColorBufferHalfFloat, EXT_color_buffer_half_float)

} // namespace mozilla
