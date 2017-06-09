/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

#ifdef FOO
#error FOO is already defined! We use FOO() macros to keep things succinct in this file.
#endif

namespace mozilla {

WebGLExtensionCompressedTextureS3TC::WebGLExtensionCompressedTextureS3TC(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    RefPtr<WebGLContext> webgl_ = webgl; // Bug 1201275
    const auto fnAdd = [&webgl_](GLenum sizedFormat, webgl::EffectiveFormat effFormat) {
        auto& fua = webgl_->mFormatUsage;

        auto usage = fua->EditUsage(effFormat);
        usage->isFilterable = true;
        fua->AllowSizedTexFormat(sizedFormat, usage);

        webgl_->mCompressedTextureFormats.AppendElement(sizedFormat);
    };

#define FOO(x) LOCAL_GL_ ## x, webgl::EffectiveFormat::x

    fnAdd(FOO(COMPRESSED_RGB_S3TC_DXT1_EXT));
    fnAdd(FOO(COMPRESSED_RGBA_S3TC_DXT1_EXT));
    fnAdd(FOO(COMPRESSED_RGBA_S3TC_DXT3_EXT));
    fnAdd(FOO(COMPRESSED_RGBA_S3TC_DXT5_EXT));

#undef FOO
}

WebGLExtensionCompressedTextureS3TC::~WebGLExtensionCompressedTextureS3TC()
{
}

bool
WebGLExtensionCompressedTextureS3TC::IsSupported(const WebGLContext* webgl)
{
   gl::GLContext* gl = webgl->GL();
   if (gl->IsExtensionSupported(gl::GLContext::EXT_texture_compression_s3tc))
      return true;

   return gl->IsExtensionSupported(gl::GLContext::EXT_texture_compression_dxt1) &&
          gl->IsExtensionSupported(gl::GLContext::ANGLE_texture_compression_dxt3) &&
          gl->IsExtensionSupported(gl::GLContext::ANGLE_texture_compression_dxt5);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureS3TC, WEBGL_compressed_texture_s3tc)

} // namespace mozilla
