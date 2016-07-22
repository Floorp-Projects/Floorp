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

WebGLExtensionCompressedTexturePVRTC::WebGLExtensionCompressedTexturePVRTC(WebGLContext* webgl)
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

    fnAdd(FOO(COMPRESSED_RGB_PVRTC_4BPPV1));
    fnAdd(FOO(COMPRESSED_RGB_PVRTC_2BPPV1));
    fnAdd(FOO(COMPRESSED_RGBA_PVRTC_4BPPV1));
    fnAdd(FOO(COMPRESSED_RGBA_PVRTC_2BPPV1));

#undef FOO
}

WebGLExtensionCompressedTexturePVRTC::~WebGLExtensionCompressedTexturePVRTC()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTexturePVRTC, WEBGL_compressed_texture_pvrtc)

} // namespace mozilla
