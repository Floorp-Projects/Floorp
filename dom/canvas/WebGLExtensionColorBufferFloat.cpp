/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

#ifdef FOO
#error FOO is already defined! We use FOO() macros to keep things succinct in this file.
#endif

namespace mozilla {

WebGLExtensionColorBufferFloat::WebGLExtensionColorBufferFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");

    auto& fua = webgl->mFormatUsage;

    auto fnUpdateUsage = [&fua](GLenum sizedFormat, webgl::EffectiveFormat effFormat) {
        auto usage = fua->EditUsage(effFormat);
        usage->SetRenderable();
        fua->AllowRBFormat(sizedFormat, usage);
    };

#define FOO(x) fnUpdateUsage(LOCAL_GL_ ## x, webgl::EffectiveFormat::x)

    // The extension doesn't actually add RGB32F; only RGBA32F.
    FOO(RGBA32F);

#undef FOO
}

WebGLExtensionColorBufferFloat::~WebGLExtensionColorBufferFloat()
{
}

bool
WebGLExtensionColorBufferFloat::IsSupported(const WebGLContext* webgl)
{
    const auto& gl = webgl->gl;
    if (gl->IsANGLE()) {
        // ANGLE supports this, but doesn't have a way to advertize its support,
        // since it's compliant with WEBGL_color_buffer_float's clamping, but not
        // EXT_color_buffer_float.
        // TODO: This probably isn't necessary anymore.
        return true;
    }

    return gl->IsSupported(gl::GLFeature::renderbuffer_color_float) &&
           gl->IsSupported(gl::GLFeature::frag_color_float);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionColorBufferFloat, WEBGL_color_buffer_float)

} // namespace mozilla
