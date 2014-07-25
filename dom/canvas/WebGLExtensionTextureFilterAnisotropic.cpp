/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

WebGLExtensionTextureFilterAnisotropic::WebGLExtensionTextureFilterAnisotropic(WebGLContext* context)
    : WebGLExtensionBase(context)
{
}

WebGLExtensionTextureFilterAnisotropic::~WebGLExtensionTextureFilterAnisotropic()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureFilterAnisotropic)
