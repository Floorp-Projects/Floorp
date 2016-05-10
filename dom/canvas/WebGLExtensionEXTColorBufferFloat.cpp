/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionEXTColorBufferFloat::WebGLExtensionEXTColorBufferFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");

    auto& fua = webgl->mFormatUsage;

    auto fnUpdateUsage = [&fua](GLenum sizedFormat, webgl::EffectiveFormat effFormat) {
        auto usage = fua->EditUsage(effFormat);
        usage->isRenderable = true;
        fua->AllowRBFormat(sizedFormat, usage);
    };

#define FOO(x) fnUpdateUsage(LOCAL_GL_ ## x, webgl::EffectiveFormat::x)

    FOO(R16F);
    FOO(RG16F);
    FOO(RGBA16F);

    FOO(R32F);
    FOO(RG32F);
    FOO(RGBA32F);

    FOO(R11F_G11F_B10F);

#undef FOO
}

/*static*/ bool
WebGLExtensionEXTColorBufferFloat::IsSupported(const WebGLContext* webgl)
{
    const gl::GLContext* gl = webgl->GL();
    return gl->IsSupported(gl::GLFeature::EXT_color_buffer_float);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionEXTColorBufferFloat, EXT_color_buffer_float)

} // namespace mozilla
