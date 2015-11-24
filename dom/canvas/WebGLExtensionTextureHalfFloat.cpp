/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

using mozilla::webgl::EffectiveFormat;

void
WebGLExtensionTextureHalfFloat::InitWebGLFormats(webgl::FormatUsageAuthority* authority)
{
    MOZ_ASSERT(authority);

    auto addFormatIfMissing = [authority](EffectiveFormat effectiveFormat)
        {
            if (!authority->GetUsage(effectiveFormat)) {
                authority->AddFormat(effectiveFormat, false, false, false, false);
            }
        };

    // Populate authority with any missing effective formats.
    addFormatIfMissing(EffectiveFormat::RGBA16F);
    addFormatIfMissing(EffectiveFormat::RGB16F);
    addFormatIfMissing(EffectiveFormat::Luminance16FAlpha16F);
    addFormatIfMissing(EffectiveFormat::Luminance16F);
    addFormatIfMissing(EffectiveFormat::Alpha16F);
}

WebGLExtensionTextureHalfFloat::WebGLExtensionTextureHalfFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    webgl::FormatUsageAuthority* authority = webgl->mFormatUsage.get();

    auto updateUsage = [authority](EffectiveFormat effectiveFormat,
                                   GLenum unpackFormat, GLenum unpackType)
        {
            webgl::FormatUsageInfo* usage = authority->GetUsage(effectiveFormat);
            MOZ_ASSERT(usage);
            usage->asTexture = true;
            authority->AddUnpackOption(unpackFormat, unpackType, effectiveFormat);
        };

    InitWebGLFormats(authority);

    // Update usage to allow asTexture and add unpack
    updateUsage(EffectiveFormat::RGBA16F             , LOCAL_GL_RGBA           , LOCAL_GL_HALF_FLOAT_OES);
    updateUsage(EffectiveFormat::RGB16F              , LOCAL_GL_RGB            , LOCAL_GL_HALF_FLOAT_OES);
    updateUsage(EffectiveFormat::Luminance16FAlpha16F, LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_HALF_FLOAT_OES);
    updateUsage(EffectiveFormat::Luminance16F        , LOCAL_GL_LUMINANCE      , LOCAL_GL_HALF_FLOAT_OES);
    updateUsage(EffectiveFormat::Alpha16F            , LOCAL_GL_ALPHA          , LOCAL_GL_HALF_FLOAT_OES);
}

WebGLExtensionTextureHalfFloat::~WebGLExtensionTextureHalfFloat()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureHalfFloat, OES_texture_half_float)

} // namespace mozilla
