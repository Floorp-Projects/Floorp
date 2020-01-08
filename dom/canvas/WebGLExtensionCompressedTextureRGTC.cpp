/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionCompressedTextureRGTC::WebGLExtensionCompressedTextureRGTC(
    WebGLContext* const webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");

  auto& fua = webgl->mFormatUsage;

  const auto fnAdd = [&](const GLenum sizedFormat,
                         const webgl::EffectiveFormat effFormat) {
    auto usage = fua->EditUsage(effFormat);
    usage->isFilterable = true;
    fua->AllowSizedTexFormat(sizedFormat, usage);

    webgl->mCompressedTextureFormats.AppendElement(sizedFormat);
  };

#define _(X) LOCAL_GL_##X, webgl::EffectiveFormat::X

  fnAdd(_(COMPRESSED_RED_RGTC1));
  fnAdd(_(COMPRESSED_SIGNED_RED_RGTC1));
  fnAdd(_(COMPRESSED_RG_RGTC2));
  fnAdd(_(COMPRESSED_SIGNED_RG_RGTC2));

#undef _
}

bool WebGLExtensionCompressedTextureRGTC::IsSupported(
    const WebGLContext* const webgl) {
  return webgl->gl->IsSupported(gl::GLFeature::texture_compression_rgtc);
}

}  // namespace mozilla
