/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

WebGLExtensionCompressedTextureATC::WebGLExtensionCompressedTextureATC(WebGLContext* context)
    : WebGLExtensionBase(context)
{
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_ATC_RGB);
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA);
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA);
}

WebGLExtensionCompressedTextureATC::~WebGLExtensionCompressedTextureATC()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureATC)
