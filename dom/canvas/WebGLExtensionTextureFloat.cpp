/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionTextureFloat::WebGLExtensionTextureFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
}

WebGLExtensionTextureFloat::~WebGLExtensionTextureFloat()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureFloat, OES_texture_float)

} // namespace mozilla
