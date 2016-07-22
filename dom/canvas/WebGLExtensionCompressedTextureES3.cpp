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

WebGLExtensionCompressedTextureES3::WebGLExtensionCompressedTextureES3(WebGLContext* webgl)
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

    fnAdd(FOO(COMPRESSED_R11_EAC));
    fnAdd(FOO(COMPRESSED_SIGNED_R11_EAC));
    fnAdd(FOO(COMPRESSED_RG11_EAC));
    fnAdd(FOO(COMPRESSED_SIGNED_RG11_EAC));
    fnAdd(FOO(COMPRESSED_RGB8_ETC2));
    fnAdd(FOO(COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2));
    fnAdd(FOO(COMPRESSED_RGBA8_ETC2_EAC));

    // sRGB support is manadatory in GL 4.3 and GL ES 3.0, which are the only
    // versions to support ETC2.
    fnAdd(FOO(COMPRESSED_SRGB8_ALPHA8_ETC2_EAC));
    fnAdd(FOO(COMPRESSED_SRGB8_ETC2));
    fnAdd(FOO(COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2));

#undef FOO
}

WebGLExtensionCompressedTextureES3::~WebGLExtensionCompressedTextureES3()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureES3, WEBGL_compressed_texture_es3)

} // namespace mozilla
