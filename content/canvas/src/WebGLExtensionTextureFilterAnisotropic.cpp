/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"

using namespace mozilla;

WebGLExtensionTextureFilterAnisotropic::WebGLExtensionTextureFilterAnisotropic(WebGLContext* context) :
    WebGLExtension(context)
{

}

WebGLExtensionTextureFilterAnisotropic::~WebGLExtensionTextureFilterAnisotropic()
{

}

NS_IMPL_ADDREF_INHERITED(WebGLExtensionTextureFilterAnisotropic, WebGLExtension)
NS_IMPL_RELEASE_INHERITED(WebGLExtensionTextureFilterAnisotropic, WebGLExtension)

DOMCI_DATA(WebGLExtensionTextureFilterAnisotropic, WebGLExtensionTextureFilterAnisotropic)

NS_INTERFACE_MAP_BEGIN(WebGLExtensionTextureFilterAnisotropic)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLExtensionTextureFilterAnisotropic)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, WebGLExtension)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLExtensionTextureFilterAnisotropic)
NS_INTERFACE_MAP_END_INHERITING(WebGLExtension)
