/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"

using namespace mozilla;

WebGLExtensionCompressedTextureS3TC::WebGLExtensionCompressedTextureS3TC(WebGLContext* context)
    : WebGLExtension(context)
{
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
    context->mCompressedTextureFormats.AppendElement(LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
}

WebGLExtensionCompressedTextureS3TC::~WebGLExtensionCompressedTextureS3TC()
{

}

NS_IMPL_ADDREF_INHERITED(WebGLExtensionCompressedTextureS3TC, WebGLExtension)
NS_IMPL_RELEASE_INHERITED(WebGLExtensionCompressedTextureS3TC, WebGLExtension)

DOMCI_DATA(WebGLExtensionCompressedTextureS3TC, WebGLExtensionCompressedTextureS3TC)

NS_INTERFACE_MAP_BEGIN(WebGLExtensionCompressedTextureS3TC)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLExtensionCompressedTextureS3TC)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, WebGLExtension)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLExtensionCompressedTextureS3TC)
NS_INTERFACE_MAP_END_INHERITING(WebGLExtension)
