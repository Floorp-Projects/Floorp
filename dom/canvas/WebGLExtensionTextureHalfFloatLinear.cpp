/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

using mozilla::webgl::EffectiveFormat;

WebGLExtensionTextureHalfFloatLinear::WebGLExtensionTextureHalfFloatLinear(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    // This update requires that the authority already be populated by
    // WebGLExtensionTextureHalfFloat.  Enabling extensions to control
    // features is a mess in WebGL

    webgl::FormatUsageAuthority* authority = webgl->mFormatUsage.get();

    auto updateUsage = [authority](EffectiveFormat effectiveFormat) {
        webgl::FormatUsageInfo* usage = authority->GetUsage(effectiveFormat);
        MOZ_ASSERT(usage);
        usage->isFilterable = true;
    };

    // Ensure require formats are initialized.
    WebGLExtensionTextureHalfFloat::InitWebGLFormats(authority);

    // Update usage to allow isFilterable
    updateUsage(EffectiveFormat::RGBA16F);
    updateUsage(EffectiveFormat::RGB16F);
    updateUsage(EffectiveFormat::Luminance16FAlpha16F);
    updateUsage(EffectiveFormat::Luminance16F);
    updateUsage(EffectiveFormat::Alpha16F);
}

WebGLExtensionTextureHalfFloatLinear::~WebGLExtensionTextureHalfFloatLinear()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureHalfFloatLinear, OES_texture_half_float_linear)

} // namespace mozilla
