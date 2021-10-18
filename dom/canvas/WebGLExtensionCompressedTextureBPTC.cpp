/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionCompressedTextureBPTC::WebGLExtensionCompressedTextureBPTC(
    WebGLContext* const webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");

  auto& fua = webgl->mFormatUsage;

  const auto fnAdd = [&](const GLenum sizedFormat,
                         const webgl::EffectiveFormat effFormat) {
    auto usage = fua->EditUsage(effFormat);
    usage->isFilterable = true;
    fua->AllowSizedTexFormat(sizedFormat, usage);
  };

#define _(X) LOCAL_GL_##X, webgl::EffectiveFormat::X

  fnAdd(_(COMPRESSED_RGBA_BPTC_UNORM));
  fnAdd(_(COMPRESSED_SRGB_ALPHA_BPTC_UNORM));
  fnAdd(_(COMPRESSED_RGB_BPTC_SIGNED_FLOAT));
  fnAdd(_(COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT));

#undef _
}

bool WebGLExtensionCompressedTextureBPTC::IsSupported(
    const WebGLContext* const webgl) {
  return webgl->gl->IsSupported(gl::GLFeature::texture_compression_bptc);
}

}  // namespace mozilla
