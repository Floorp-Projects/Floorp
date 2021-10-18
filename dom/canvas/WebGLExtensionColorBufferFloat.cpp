/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionColorBufferFloat::WebGLExtensionColorBufferFloat(
    WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
  SetRenderable(webgl::FormatRenderableState::Implicit(
      WebGLExtensionID::WEBGL_color_buffer_float));
}

void WebGLExtensionColorBufferFloat::SetRenderable(
    const webgl::FormatRenderableState state) {
  auto& fua = mContext->mFormatUsage;

  auto fnUpdateUsage = [&](GLenum sizedFormat,
                           webgl::EffectiveFormat effFormat) {
    auto usage = fua->EditUsage(effFormat);
    usage->SetRenderable(state);
    fua->AllowRBFormat(sizedFormat, usage);
  };

#define FOO(x) fnUpdateUsage(LOCAL_GL_##x, webgl::EffectiveFormat::x)

  // The extension doesn't actually add RGB32F; only RGBA32F.
  FOO(RGBA32F);

#undef FOO
}

void WebGLExtensionColorBufferFloat::OnSetExplicit() {
  SetRenderable(webgl::FormatRenderableState::Explicit());
}

bool WebGLExtensionColorBufferFloat::IsSupported(const WebGLContext* webgl) {
  if (webgl->IsWebGL2()) return false;

  const auto& gl = webgl->gl;
  return gl->IsSupported(gl::GLFeature::renderbuffer_color_float) &&
         gl->IsSupported(gl::GLFeature::frag_color_float);
}

}  // namespace mozilla
