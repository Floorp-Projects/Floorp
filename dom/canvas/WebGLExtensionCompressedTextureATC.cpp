/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionCompressedTextureATC::WebGLExtensionCompressedTextureATC(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    webgl->mCompressedTextureFormats.AppendElement(LOCAL_GL_ATC_RGB);
    webgl->mCompressedTextureFormats.AppendElement(LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA);
    webgl->mCompressedTextureFormats.AppendElement(LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA);
}

WebGLExtensionCompressedTextureATC::~WebGLExtensionCompressedTextureATC()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureATC)

} // namespace mozilla
