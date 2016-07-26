/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionTextureHalfFloatLinear::WebGLExtensionTextureHalfFloatLinear(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    auto& fua = webgl->mFormatUsage;

    fua->EditUsage(webgl::EffectiveFormat::RGBA16F)->isFilterable = true;
    fua->EditUsage(webgl::EffectiveFormat::RGB16F)->isFilterable = true;
    fua->EditUsage(webgl::EffectiveFormat::Luminance16FAlpha16F)->isFilterable = true;
    fua->EditUsage(webgl::EffectiveFormat::Luminance16F)->isFilterable = true;
    fua->EditUsage(webgl::EffectiveFormat::Alpha16F)->isFilterable = true;
}

WebGLExtensionTextureHalfFloatLinear::~WebGLExtensionTextureHalfFloatLinear()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureHalfFloatLinear, OES_texture_half_float_linear)

} // namespace mozilla
