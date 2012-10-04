/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

WebGLExtensionCompressedTextureS3TC::WebGLExtensionCompressedTextureS3TC(WebGLContext* context)
    : WebGLExtensionBase(context)
{
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
}

WebGLExtensionCompressedTextureS3TC::~WebGLExtensionCompressedTextureS3TC()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureS3TC)
