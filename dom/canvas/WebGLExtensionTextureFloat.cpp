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
WebGLExtensionTextureFloat::InitWebGLFormats(webgl::FormatUsageAuthority* authority)
{
    MOZ_ASSERT(authority);

    auto addFormatIfMissing = [authority](EffectiveFormat effectiveFormat)
        {
            if (!authority->GetUsage(effectiveFormat)) {
                authority->AddFormat(effectiveFormat, false, false, false, false);
            }
        };

    // Populate authority with any missing effective formats.
    addFormatIfMissing(EffectiveFormat::RGBA32F);
    addFormatIfMissing(EffectiveFormat::RGB32F);
    addFormatIfMissing(EffectiveFormat::Luminance32FAlpha32F);
    addFormatIfMissing(EffectiveFormat::Luminance32F);
    addFormatIfMissing(EffectiveFormat::Alpha32F);
}

WebGLExtensionTextureFloat::WebGLExtensionTextureFloat(WebGLContext* webgl)
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

    // Ensure require formats are initialized.
    InitWebGLFormats(authority);

    // Update usage to allow asTexture and add unpack
    updateUsage(EffectiveFormat::RGBA32F             , LOCAL_GL_RGBA           , LOCAL_GL_FLOAT);
    updateUsage(EffectiveFormat::RGB32F              , LOCAL_GL_RGB            , LOCAL_GL_FLOAT);
    updateUsage(EffectiveFormat::Luminance32FAlpha32F, LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_FLOAT);
    updateUsage(EffectiveFormat::Luminance32F        , LOCAL_GL_LUMINANCE      , LOCAL_GL_FLOAT);
    updateUsage(EffectiveFormat::Alpha32F            , LOCAL_GL_ALPHA          , LOCAL_GL_FLOAT);
}

WebGLExtensionTextureFloat::~WebGLExtensionTextureFloat()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureFloat, OES_texture_float)

} // namespace mozilla
