/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionCompressedTexturePVRTC::WebGLExtensionCompressedTexturePVRTC(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    webgl->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1);
    webgl->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1);
    webgl->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1);
    webgl->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1);
}

WebGLExtensionCompressedTexturePVRTC::~WebGLExtensionCompressedTexturePVRTC()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTexturePVRTC)

} // namespace mozilla
